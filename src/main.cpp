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
    bool show_globals = false;
    bool get_layout = false;
};

wxIMPLEMENT_APP_CONSOLE(MainApp);

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-pdf");
    parser.AddParam("input-bls");
    parser.AddSwitch("d", "show-debug", "Show Debug Variables");
    parser.AddSwitch("g", "show-globals", "Show Global Variables");
    parser.AddSwitch("l", "get-layout", "Halt On $setlayout");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    input_pdf = parser.GetParam(0).ToStdString();
    input_bls = parser.GetParam(1).ToStdString();
    show_debug = parser.FoundSwitch("d") == wxCMD_SWITCH_ON;
    show_globals = parser.FoundSwitch("g") == wxCMD_SWITCH_ON;
    get_layout = parser.FoundSwitch("l") == wxCMD_SWITCH_ON;
    return true;
}

int MainApp::OnRun() {
    Json::Value result = Json::objectValue;

    auto output_error = [&](const std::string &message) {
        result["error"] = message;
        std::cout << result;
        return 1;
    };

    try {
        pdf_document my_doc(input_pdf);
        reader my_reader(my_doc, bill_layout_script::from_file(input_bls));
        if (get_layout) {
            my_reader.add_flags(READER_HALT_ON_SETLAYOUT);
        }
        my_reader.start();

        const auto &out = my_reader.get_output();

        Json::Value &json_values = result["values"] = Json::arrayValue;
        
        auto write_var = [](Json::Value &table, const std::string &name, const variable &var) {
            if (table.isNull()) table = Json::objectValue;
            auto &json_arr = table[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(var.str());
        };

        for (auto &[key, var] : out.values) {
            if (key.name.front() == '_' && !show_debug) {
                continue;
            }
            if (key.table_index == variable_key::global_index) {
                if (show_globals) {
                    write_var(result["globals"], key.name, var);
                }
            } else {
                while (json_values.size() <= key.table_index) {
                    json_values.append(Json::objectValue);
                }
                write_var(json_values[key.table_index], key.name, var);
            }
        }

        for (auto &v : out.warnings) {
            Json::Value &warnings = result["warnings"];
            if (warnings.isNull()) warnings = Json::arrayValue;
            warnings.append(v);
        }

        auto &json_layouts = result["layouts"] = Json::arrayValue;
        for (auto &l : out.layouts) {
            json_layouts.append(l.string());
        }

        std::cout << result;
    } catch (const std::exception &error) {
        return output_error(error.what());
    } catch (...) {
        return output_error("Errore sconosciuto");
    }

    return 0;
}