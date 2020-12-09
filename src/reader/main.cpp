#include <iostream>
#include <fstream>

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>

#include "reader.h"

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

    wxLocale loc = wxLANGUAGE_DEFAULT;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_OPTION, "p", "pdf", "input pdf", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_OPTION, "l", "layout-dir", "layout directory", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "d", "debug", "debug", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "s", "script", "script controllo", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
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
    return true;
}

int MainApp::OnRun() {
    Json::Value result = Json::objectValue;
    result["error"] = false;

    auto output_error = [&](const std::string &message) {
        result["error"] = true;
        result["message"] = message;
        std::cout << result;
        return 1;
    };

    if (!wxFileExists(file_pdf)) {
        return output_error(wxString::Format("Impossibile aprire il file pdf %s", file_pdf).ToStdString());
    }

    reader m_reader;

    std::ifstream ifs;
    bool input_stdin = false;
    bool in_file_layout = true;

    if (input_file == "-") {
#       if defined(WIN32) || defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#       endif
        input_stdin = true;
    } else {
        if (!wxFileExists(input_file)) {
            return output_error(wxString::Format("Impossibile aprire il file layout %s", input_file).ToStdString());
        }
        ifs.open(input_file.ToStdString(), std::ifstream::binary | std::ifstream::in);
        if (layout_dir.empty()) {
            layout_dir = wxFileName(input_file).GetPath();
        }
    }

    try {
        m_reader.open_pdf(file_pdf.ToStdString());

        if (exec_script) {
            if (input_stdin) {
                m_reader.exec_program(std::cin);
            } else {
                m_reader.exec_program(ifs);
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
                input_stdin = false;
            }
        }
        if (in_file_layout) {
            if (input_stdin) {
                m_reader.exec_program(std::cin);
            } else {
                m_reader.exec_program(ifs);
                ifs.close();
            }
        }
    } catch (const layout_error &error) {
        return output_error(error.message);
    }
#ifdef NDEBUG
    catch (...) {
        return output_error("Errore sconosciuto");
    }
#endif

    m_reader.save_output(result, debug);
    std::cout << result;

    return 0;
}