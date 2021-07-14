#include <iostream>
#include <filesystem>

#include "tao/json.hpp"
#include "cxxopts.hpp"

#include "parser.h"
#include "reader.h"
#include "utils.h"

using namespace bls;

struct MainApp {
    int run();

    std::filesystem::path input_pdf;
    std::filesystem::path input_bls;

    bool show_debug = false;
    bool show_globals = false;
    bool get_layout = false;
    bool use_cache = false;
};

int MainApp::run() {
    int retcode = 0;
    tao::json::value result = tao::json::empty_object;

    reader my_reader;

    try {
        pdf_document my_doc;
        
        if (!input_pdf.empty()) {
            my_doc.open(input_pdf);
            my_reader.set_document(my_doc);
        }
        
        if (use_cache) my_reader.add_flag(reader_flags::USE_CACHE);
        if (get_layout) my_reader.add_flag(reader_flags::HALT_ON_SETLAYOUT);
        my_reader.add_layout(input_bls);
        my_reader.start();

        tao::json::value &json_values = result["values"] = tao::json::empty_array;
        
        auto write_var = [](tao::json::value &table, const std::string &name, const variable &var) {
            if (table.is_null()) table = tao::json::empty_object;
            auto &json_arr = table[name];
            if (json_arr.is_null()) json_arr = tao::json::empty_array;
            json_arr.push_back(var.as_string());
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
                while (json_values.get_array().size() <= key.table_index) {
                    json_values.push_back(tao::json::empty_object);
                }
                write_var(json_values[key.table_index], key.name, var);
            }
        }

        for (auto &v : my_reader.get_notes()) {
            tao::json::value &notes = result["notes"];
            if (notes.is_null()) notes = tao::json::empty_array;
            notes.push_back(v);
        }

        auto &json_layouts = result["layouts"] = tao::json::empty_array;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.push_back(l.string());
        }

        result["errcode"] = 0;
    } catch (const layout_runtime_error &error) {
        result["error"] = error.what();
        result["errcode"] = error.errcode;

        auto &json_layouts = result["layouts"] = tao::json::empty_array;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.push_back(l.string());
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

    tao::json::to_stream(std::cout, result, 4);
    return retcode;
}

int main(int argc, char **argv) {
    MainApp app;

    try {
        cxxopts::Options options(argv[0], "Executes bls files");

        options.positional_help("Input-bls-File");

        options.add_options()
            ("input-bls", "Input bls File", cxxopts::value(app.input_bls))
            ("p,input-pdf", "Input pdf File", cxxopts::value(app.input_pdf))
            ("d,show-debug", "Show Debug Variables", cxxopts::value(app.show_debug))
            ("g,show-globals", "Show Global Variables", cxxopts::value(app.show_globals))
            ("k,halt-setlayout", "Halt On Setlayout", cxxopts::value(app.get_layout))
            ("c,use-cache", "Use Script Cache", cxxopts::value(app.use_cache))
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