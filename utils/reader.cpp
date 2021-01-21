#include <iostream>
#include <fstream>

#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <json/json.h>

#include "parser.h"
#include "reader.h"
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

Json::Value save_output(const reader_output &out, Json::Value &root, bool debug) {
    auto write_values = [&](const variable_map &values) {
        Json::Value ret = Json::objectValue;
        for (auto &[name, var] : values) {
            if (name.front() == '_' && !debug) {
                continue;
            }
            auto &json_arr = ret[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(var.str());
        }
        return ret;
    };

    root["globals"] = write_values(out.globals);
    
    Json::Value &json_values = root["values"] = Json::arrayValue;
    for (auto &v : out.values) {
        json_values.append(write_values(v));
    }

    for (auto &v : out.warnings) {
        Json::Value &warnings = root["warnings"];
        if (warnings.isNull()) warnings = Json::arrayValue;
        warnings.append(v);
    }

    return root;
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

    bytecode in_code;
    bool in_file_layout = true;
    reader my_reader;
    const auto &my_output = my_reader.get_output();

    if (compile) {
        in_code = parser((input_file == "-") ?
            bill_layout_script::from_stream(std::cin) :
            bill_layout_script::from_file(input_file.ToStdString())).get_bytecode();
    } else if (input_file == "-") {
#ifdef _WIN32
        setmode(fileno(stdin), O_BINARY);
#endif
        std::cin >> in_code;
    } else {
        if (!wxFileExists(input_file)) {
            return output_error(wxString::Format("Impossibile aprire il file layout %s", input_file).ToStdString());
        }
        in_code = bytecode::from_file(input_file.ToStdString());
        if (layout_dir.empty()) {
            layout_dir = wxFileName(input_file).GetPath();
        }
    }

    try {
        pdf_document my_doc(file_pdf.ToStdString());
        my_reader.set_document(my_doc);

        if (exec_script) {
            my_reader.exec_program(std::move(in_code));
            in_file_layout = false;
        }
        if (!layout_dir.empty()) {
            auto layout_it = my_output.globals.find("layout");
            if (layout_it != my_output.globals.end()) {
                wxFileName input_file2 = layout_dir + wxFileName::GetPathSeparator() + layout_it->second.str();
                input_file2.SetExt("out");
                if (!input_file2.Exists()) {
                    return output_error(wxString::Format("Impossibile aprire il file layout %s", input_file2.GetFullPath()).ToStdString());
                }
                in_code = bytecode::from_file(input_file2.GetFullPath().ToStdString());
                in_file_layout = true;
            }
        }
        if (in_file_layout) {
            my_reader.exec_program(std::move(in_code));
        }
    } catch (const std::exception &error) {
        return output_error(error.what());
    } catch (...) {
        return output_error("Errore sconosciuto");
    }

    std::cout << save_output(my_output, result, debug);

    return 0;
}