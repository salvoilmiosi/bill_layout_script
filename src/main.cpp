#include <iostream>
#include <filesystem>

#include <json/json.h>

#include "parser.h"
#include "reader.h"
#include "utils.h"
#include "intl.h"

struct MainApp {
    int run();

    std::filesystem::path input_pdf;
    std::filesystem::path input_bls;

    std::string selected_locale;

    bool show_debug = false;
    bool show_globals = false;
    bool get_layout = false;
    bool use_cache = false;
    bool parse_recursive = false;
};

int MainApp::run() {
    if (!intl::set_language(selected_locale)) {
        std::cerr << "Lingua non supportata: " << selected_locale << '\n';
        return -1;
    }

    int retcode = 0;
    Json::Value result = Json::objectValue;

    reader my_reader;

    try {
        pdf_document my_doc;
        
        if (!input_pdf.empty()) {
            my_doc.open(input_pdf);
            my_reader.set_document(my_doc);
        }
        
        if (parse_recursive) my_reader.add_flags(reader_flags::RECURSIVE);
        if (use_cache) my_reader.add_flags(reader_flags::USE_CACHE);
        if (get_layout) my_reader.add_flags(reader_flags::HALT_ON_SETLAYOUT);
        my_reader.add_layout(input_bls);
        my_reader.start();

        Json::Value &json_values = result["values"] = Json::arrayValue;
        
        auto write_var = [](Json::Value &table, const std::string &name, const variable &var) {
            if (table.isNull()) table = Json::objectValue;
            auto &json_arr = table[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(var.as_string());
        };

        for (auto &[key, var] : my_reader.get_values()) {
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

        for (auto &v : my_reader.get_notes()) {
            Json::Value &notes = result["notes"];
            if (notes.isNull()) notes = Json::arrayValue;
            notes.append(v);
        }

        auto &json_layouts = result["layouts"] = Json::arrayValue;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.append(l.string());
        }

        result["errcode"] = 0;
    } catch (const layout_runtime_error &error) {
        result["error"] = error.what();
        result["errcode"] = error.errcode;

        auto &json_layouts = result["layouts"] = Json::arrayValue;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.append(l.string());
        }
        
        retcode = 1;
    } catch (const layout_error &error) {
        result["error"] = error.what();
        result["errcode"] = -1;
        retcode = 2;
    } catch (const std::exception &error) {
        result["error"] = error.what();
        result["errcode"] = -1;
        retcode = 3;
    } catch (...) {
        result["error"] = "Errore sconosciuto";
        result["errcode"] = -1;
        retcode = 4;
    }

    std::cout << result;
    return retcode;
}

#ifndef USE_CXXOPTS
#include <wx/app.h>
#include <wx/cmdline.h>
struct WxMainApp : public wxAppConsole, public MainApp {
    virtual int OnRun() override {
        return MainApp::run();
    }

    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;
};

wxIMPLEMENT_APP_CONSOLE(WxMainApp);

void WxMainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls");
    parser.AddOption("p", "input-pdf", "Input PDF");
    parser.AddOption("l", "language", "Language");
    parser.AddSwitch("d", "show-debug", "Show Debug Variables");
    parser.AddSwitch("g", "show-globals", "Show Global Variables");
    parser.AddSwitch("h", "halt-setlayout", "Halt On Setlayout");
    parser.AddSwitch("c", "use-cache", "Use Script Cache");
    parser.AddSwitch("r", "recursive-imports", "Recursive Imports");
}

bool WxMainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    wxString str;

    input_bls = parser.GetParam(0).ToStdString();
    show_debug = parser.FoundSwitch("d") == wxCMD_SWITCH_ON;
    show_globals = parser.FoundSwitch("g") == wxCMD_SWITCH_ON;
    get_layout = parser.FoundSwitch("h") == wxCMD_SWITCH_ON;
    use_cache = parser.FoundSwitch("c") == wxCMD_SWITCH_ON;
    parse_recursive = parser.FoundSwitch("r") == wxCMD_SWITCH_ON;

    if (parser.Found("p", &str)) {
        input_pdf = str.ToStdString();
    }

    if (parser.Found("l", &str)) {
        selected_locale = str.ToStdString();
    }
    
    return true;
}

#else
#include "cxxopts.hpp"

int main(int argc, char **argv) {
    MainApp app;

    try {
        cxxopts::Options options(argv[0], "Executes bls files");

        options.positional_help("Input-bls-File");

        options.add_options()
            ("input-bls", "Input bls File", cxxopts::value(app.input_bls))
            ("p,input-pdf", "Input pdf File", cxxopts::value(app.input_pdf))
            ("l,language", "Language", cxxopts::value(app.selected_locale))
            ("d,show-debug", "Show Debug Variables", cxxopts::value(app.show_debug))
            ("g,show-globals", "Show Global Variables", cxxopts::value(app.show_globals))
            ("k,halt-setlayout", "Halt On Setlayout", cxxopts::value(app.get_layout))
            ("c,use-cache", "Use Script Cache", cxxopts::value(app.use_cache))
            ("r,recursive-imports", "Recursive Imports", cxxopts::value(app.parse_recursive))
            ("h,help", "Print Help");

        options.parse_positional({"input-bls"});

        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        if (!result.count("input-bls")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
    } catch (const cxxopts::OptionException &e) {
        std::cerr << "Errore nel parsing delle opzioni: " << e.what() << std::endl;
        return -1;
    }

    return app.run();
}

#endif