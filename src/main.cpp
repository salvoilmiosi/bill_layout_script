#include <iostream>
#include <filesystem>

#include <boost/json.hpp>
#include <boost/program_options.hpp>

#include "parser.h"
#include "reader.h"
#include "utils.h"

using namespace bls;
using namespace boost;

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
    json::object result;

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

        json::array json_values;
        
        auto write_var = [](json::value &table, const std::string &name, const variable &var) {
            if (table.is_null()) table = json::object();
            auto &json_arr = table.as_object()[name];
            if (json_arr.is_null()) json_arr = json::array();
            json_arr.as_array().emplace_back(var.as_string());
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
                    json_values.push_back(json::object());
                }
                write_var(json_values[key.table_index], key.name, var);
            }
        }

        result["values"] = json_values;

        if (!my_reader.get_notes().empty()) {
            json::array json_notes;
            for (auto &v : my_reader.get_notes()) {
                json_notes.emplace_back(v);
            }
            result["notes"] = json_notes;
        }

        json::array json_layouts;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.emplace_back(l.string());
        }
        result["layouts"] = json_layouts;

        result["errcode"] = 0;
    } catch (const layout_runtime_error &error) {
        result["error"] = error.what();
        result["errcode"] = error.errcode;

        json::array json_layouts;
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.emplace_back(l.string());
        }
        result["layouts"] = json_layouts;
        
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

int main(int argc, char **argv) {
    try {
        MainApp app;

        namespace po = program_options;

        po::options_description desc("Allowed options");
        po::positional_options_description pos;
        pos.add("input-bls", -1);

        desc.add_options()
            ("help,h", "Print Help")
            ("input-bls", po::value(&app.input_bls), "Input bls File")
            ("input-pdf,p", po::value(&app.input_pdf), "Input pdf File")
            ("show-debug,d", po::value(&app.show_debug), "Show Debug Variables")
            ("show-globals,g", po::value(&app.show_globals), "Show Global Variables")
            ("halt-setlayout,k", po::value(&app.get_layout), "Halt On Setlayout")
            ("use-cache,c", po::value(&app.use_cache), "Use Script Cache")
        ;

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).
            options(desc).positional(pos).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        if (!vm.count("input-bls")) {
            std::cout << "Required Input bls" << std::endl;
            return 0;
        }

        return app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}