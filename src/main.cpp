#include <iostream>
#include <filesystem>

#include <boost/program_options.hpp>

#include "parser.h"
#include "reader.h"
#include "utils.h"
#include "json_value.h"

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

    unsigned indent_size = 4;
};

static json::value variable_to_value(const variable &var) {
    if (var.is_null()) {
        return {};
    } else if (var.is_array()) {
        return var.as_array()
            | std::views::transform(variable_to_value)
            | util::range_to<json::array>;
    } else {
        return var.as_string();
    }
}

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
        
        auto write_table = [&](const variable_map &table) {
            json::object out;
            for (const auto &[key, var] : table) {
                if (!show_debug && key.front() == '_') {
                    continue;
                }
                out[key] = variable_to_value(var);
            }
            return out;
        };

        auto &json_values = result["values"] = json::array();
        for (const auto &table : my_reader.get_values()) {
            json_values.as_array().push_back(write_table(table));
        }

        if (show_globals) {
            result["globals"] = write_table(my_reader.get_globals());
        }

        if (!my_reader.get_notes().empty()) {
            auto &json_notes = result["notes"] = json::array();
            for (auto &v : my_reader.get_notes()) {
                json_notes.emplace_back(v);
            }
        }

        auto &json_layouts = result["layouts"] = json::array();
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.emplace_back(l.string());
        }

        result["errcode"] = 0;
    } catch (const layout_runtime_error &error) {
        result["error"] = error.what();
        result["errcode"] = error.errcode;

        auto &json_layouts = result["layouts"] = json::array();
        for (auto &l : my_reader.get_layouts()) {
            json_layouts.emplace_back(l.string());
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

    json::printer(std::cout, indent_size)(result);
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
            ("show-debug,d", po::bool_switch(&app.show_debug), "Show Debug Variables")
            ("show-globals,g", po::bool_switch(&app.show_globals), "Show Global Variables")
            ("halt-setlayout,k", po::bool_switch(&app.get_layout), "Halt On Setlayout")
            ("use-cache,c", po::bool_switch(&app.use_cache), "Use Script Cache")
            ("indent-size,i", po::value(&app.indent_size), "Indentation Size")
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