#include <iostream>
#include <filesystem>

#include <cxxopts.hpp>

#include "parser.h"
#include "reader.h"

#include "utils/json_value.h"

using namespace bls;

struct MainApp {
    int run();

    std::filesystem::path input_pdf;
    std::filesystem::path input_bls;

    bool find_layout = false;

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
    int retcode = 1;
    json::object result;

    reader my_reader;

    try {
        pdf_document my_doc;
        
        if (!input_pdf.empty()) {
            my_doc.open(input_pdf);
            my_reader.set_document(my_doc);
        }
        
        if (find_layout) my_reader.add_flag(reader_flags::FIND_LAYOUT);
        my_reader.add_layout(layout_box_list(input_bls));
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
        retcode = 0;
    } catch (const scripted_error &error) {
        result["error"] = error.what();
        result["errcode"] = error.errcode;

        result["layouts"] = my_reader.get_layouts()
            | std::views::transform([](const std::filesystem::path &path) { return path.string(); })
            | util::range_to<json::array>;
    } catch (const std::exception &error) {
        result["error"] = error.what();
        result["errcode"] = -1;
    } catch (...) {
        result["error"] = intl::translate("UNKNOWN_ERROR");
        result["errcode"] = -2;
    }

    json::printer(std::cout, indent_size)(result);
    return retcode;
}

int main(int argc, char **argv) {
    try {
        MainApp app;
        
        cxxopts::Options options(argv[0], intl::translate("PROGRAM_DESCRIPTION"));

        options.add_options()
            ("input-bls",   intl::translate("BLS_INPUT_FILE"),      cxxopts::value(app.input_bls))
            ("p,input-pdf", intl::translate("PDF_INPUT_FILE"),      cxxopts::value(app.input_pdf))
            ("find-layout", intl::translate("FIND_LAYOUT"),         cxxopts::value(app.find_layout))
            ("indent-size", intl::translate("INDENTATION_SIZE"),    cxxopts::value(app.indent_size))
            ("h,help",      intl::translate("PRINT_HELP"))
        ;

        options.positional_help(intl::translate("BLS_INPUT_FILE"));
        options.parse_positional({"input-bls"});

        auto results = options.parse(argc, argv);

        if (results.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        if (!results.count("input-bls")) {
            std::cout << intl::translate("REQUIRED_INPUT_BLS") << std::endl;
            return 0;
        }

        return app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}