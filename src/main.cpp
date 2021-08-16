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
                out[key] = variable_to_value(var);
            }
            return out;
        };

        result["values"] = my_reader.get_values()
            | std::views::transform(write_table)
            | util::range_to<json::array>;

        if (!my_reader.get_notes().empty()) {
            result["notes"] = my_reader.get_notes() | util::range_to<json::array>;
        }

        result["layouts"] = my_reader.get_layouts()
            | std::views::transform([](const std::filesystem::path &path) { return path.string(); })
            | util::range_to<json::array>;

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
        result["error"] = intl::format("UNKNOWN_ERROR");
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

        po::options_description desc(intl::format("OPTIONS"));
        po::positional_options_description pos;
        pos.add("input-bls", -1);

        desc.add_options()
            ("help,h", intl::format("PRINT_HELP").c_str())
            ("input-bls", po::value(&app.input_bls), intl::format("BLS_INPUT_FILE").c_str())
            ("input-pdf,p", po::value(&app.input_pdf), intl::format("PDF_INPUT_FILE").c_str())
            ("halt-setlayout", po::bool_switch(&app.get_layout), intl::format("HALT_ON_SETLAYOUT").c_str())
            ("use-cache,c", po::bool_switch(&app.use_cache), intl::format("USE_CACHE").c_str())
            ("indent-size", po::value(&app.indent_size), intl::format("INDENTATION_SIZE").c_str())
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
            std::cout << intl::format("REQUIRED_INPUT_BLS") << std::endl;
            return 0;
        }

        return app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}