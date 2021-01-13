#include <iostream>
#include <fstream>

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "reader.h"
#include "subprocess.h"
#include "utils.h"

class MainApp : public wxAppConsole {
public:
    virtual int OnRun() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    wxString input_file;
    wxString file_pdf;
    wxString layout_dir;
    bool exec_script = false;
    bool debug = false;
    bool compile = false;

    wxLocale loc = wxLANGUAGE_DEFAULT;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_OPTION, "p", "pdf", "input pdf", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_OPTION, "l", "layout-dir", "layout directory", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "d", "debug", "debug", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "s", "script", "script controllo", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "c", "compile", "compila script", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input layout", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) input_file = parser.GetParam(0);
    parser.Found("p", &file_pdf);
    parser.Found("l", &layout_dir);
    exec_script = parser.Found("s");
    debug = parser.Found("d");
    compile = parser.Found("c");
    return true;
}

int MainApp::OnRun() {
    Json::Value result = Json::objectValue;

    auto output_error = [&](const std::string &message) {
        result["error"] = message;
        std::cout << result;
        return 1;
    };

    if (!wxFileExists(file_pdf)) {
        return output_error(wxString::Format("Impossibile aprire il file pdf %s", file_pdf).ToStdString());
    }

    reader m_reader;

    std::istream *in_stream;
    std::ifstream ifs;
    subprocess proc_compiler;
    bool in_file_layout = true;

    if (compile) {
        auto cmd_str = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPathWithSep() + "compiler";
        proc_compiler.open(arguments(cmd_str, "-o", "-", input_file));
        if (input_file == "-") {
            std::copy(
                std::istreambuf_iterator<char>(std::cin),
                std::istreambuf_iterator<char>(),
                std::ostreambuf_iterator<char>(proc_compiler.stream_in)
            );
            proc_compiler.stream_in.close();
        }
        in_stream = &proc_compiler.stream_out;
        std::string compile_error = read_all(proc_compiler.stream_err);
        if (!compile_error.empty()) {
            return output_error(compile_error);
        }
    } else if (input_file == "-") {
#ifdef _WIN32
        setmode(fileno(stdin), O_BINARY);
#endif
        in_stream = &std::cin;
    } else {
        if (!wxFileExists(input_file)) {
            return output_error(wxString::Format("Impossibile aprire il file layout %s", input_file).ToStdString());
        }
        ifs.open(input_file.ToStdString(), std::ifstream::binary | std::ifstream::in);
        in_stream = &ifs;
        if (layout_dir.empty()) {
            layout_dir = wxFileName(input_file).GetPath();
        }
    }

    try {
        m_reader.open_pdf(file_pdf.ToStdString());

        if (exec_script) {
            m_reader.exec_program(*in_stream);
            if (ifs.is_open()) {
                ifs.close();
            }

            in_file_layout = false;
        }
        if (!layout_dir.empty()) {
            const auto &layout_path = m_reader.get_global("layout");
            if (!layout_path.empty()) {
                wxFileName input_file2 = layout_dir + wxFileName::GetPathSeparator() + layout_path.str();
                input_file2.SetExt("out");
                if (!input_file2.Exists()) {
                    return output_error(wxString::Format("Impossibile aprire il file layout %s", input_file2.GetFullPath()).ToStdString());
                }
                ifs.open(input_file2.GetFullPath().ToStdString(), std::ifstream::binary | std::ifstream::in);
                in_file_layout = true;
                in_stream = &ifs;
            }
        }
        if (in_file_layout) {
            m_reader.exec_program(*in_stream);
            if (ifs.is_open()) {
                ifs.close();
            }
        }
    } catch (const layout_error &error) {
        return output_error(error.message);
    } catch (const std::exception &error) {
        return output_error(error.what());
    } catch (...) {
        return output_error("Errore sconosciuto");
    }

    m_reader.save_output(result, debug);
    std::cout << result;

    return 0;
}