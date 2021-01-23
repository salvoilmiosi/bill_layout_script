#include <iostream>
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
    std::filesystem::path input_pdf;
    std::filesystem::path input_bls;

    bool show_debug = false;

    wxLocale loc = wxLANGUAGE_DEFAULT;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_SWITCH, "d", "show-debug", "Show Debug Variables", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input-pdf", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input-bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_OPTION_MANDATORY },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 2) {
        input_pdf = parser.GetParam(0).ToStdString();
        input_bls = parser.GetParam(1).ToStdString();
    }
    show_debug = parser.Found("d");
    return true;
}

int MainApp::OnRun() {
    Json::Value result = Json::objectValue;

    auto output_error = [&](const std::string &message) {
        result["error"] = message;
        std::cout << result;
        return 1;
    };
    
    auto write_values = [&](const variable_map &values) {
        Json::Value ret = Json::objectValue;
        for (auto &[name, var] : values) {
            if (name.front() == '_' && !show_debug) {
                continue;
            }
            auto &json_arr = ret[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(var.str());
        }
        return ret;
    };

    try {
        pdf_document my_doc(input_pdf);
        reader my_reader(my_doc, bill_layout_script::from_file(input_bls));
        my_reader.start();

        const auto &out = my_reader.get_output();

        result["globals"] = write_values(out.globals);
        
        Json::Value &json_values = result["values"] = Json::arrayValue;
        for (auto &v : out.values) {
            json_values.append(write_values(v));
        }

        for (auto &v : out.warnings) {
            Json::Value &warnings = result["warnings"];
            if (warnings.isNull()) warnings = Json::arrayValue;
            warnings.append(v);
        }

        auto &json_layouts = result["layouts"] = Json::arrayValue;
        for (auto &l : out.layouts) {
            json_layouts.append(l);
        }

        std::cout << result;
    } catch (const std::exception &error) {
        return output_error(error.what());
    } catch (...) {
        return output_error("Errore sconosciuto");
    }

    return 0;
}