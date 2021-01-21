#include <iostream>
#include <fstream>
#include <filesystem>

#include <wx/app.h>
#include <wx/cmdline.h>

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
    std::filesystem::path input_file;
    std::filesystem::path file_pdf;

    bool debug = false;

    wxLocale loc = wxLANGUAGE_DEFAULT;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_OPTION, "p", "pdf", "input pdf", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_OPTION, "l", "layout-dir", "layout directory", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_SWITCH, "d", "debug", "debug", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input layout", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    wxString str;

    if (parser.GetParamCount() >= 1) {
        input_file = parser.GetParam(0).ToStdString();
    }
    if (parser.Found("p", &str)) {
        file_pdf = str.ToStdString();
    }
    debug = parser.Found("d");
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

    auto &json_layouts = root["layouts"] = Json::arrayValue;
    for (auto &l : out.layouts) {
        json_layouts.append(l);
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

    try {
        pdf_document my_doc(file_pdf);
        reader my_reader(my_doc, bill_layout_script::from_file(input_file));
        my_reader.start();

        std::cout << save_output(my_reader.get_output(), result, debug);
    } catch (const std::exception &error) {
        return output_error(error.what());
    } catch (...) {
        return output_error("Errore sconosciuto");
    }

    return 0;
}