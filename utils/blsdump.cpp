#include <iostream>
#include <filesystem>

#include <boost/program_options.hpp>

#include "parser.h"
#include "binary_bls.h"
#include "bytecode_printer.h"

struct MainApp {
    int run();

    std::filesystem::path input_file;
    std::filesystem::path output_cache;

    bool do_read_cache;
};

int main(int argc, char **argv) {
    try {
        MainApp app;

        namespace po = boost::program_options;

        po::options_description desc(intl::format("OPTIONS"));
        po::positional_options_description pos;
        pos.add("input-bls", -1);

        desc.add_options()
            ("help,h", intl::format("PRINT_HELP").c_str())
            ("input-bls", po::value(&app.input_file), intl::format("BLS_INPUT_FILE").c_str())
            ("read-cache,c", po::bool_switch(&app.do_read_cache), intl::format("READ_CACHE").c_str())
            ("output-cache,o", po::value(&app.output_cache), intl::format("OUT_CACHE_FILE").c_str())
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
        std::cerr << e.what() << std::endl;
        return -1;
    }
}

using namespace bls;

int MainApp::run() {
    try {
        bytecode code;

        if (!do_read_cache) {
            parser my_parser;
            my_parser.read_layout(input_file, layout_box_list::from_file(input_file));
            code = std::move(my_parser).get_bytecode();
        } else {
            code = binary::read(input_file);
        }

        if (!output_cache.empty()) {
            binary::write(code, output_cache);
        }
        for (auto line = code.begin(); line != code.end(); ++line) {
            visit_command(util::overloaded{
                [](command_tag<opcode::BOXNAME>, const std::string &line) {
                    std::cout << "### " << line;
                },
                [](command_tag<opcode::COMMENT>, const comment_line &line) {
                    std::cout << print_args(line);
                },
                [](command_tag<opcode::LABEL>, const jump_label &label) {
                    std::cout << print_args(label) << ':';
                },
                []<opcode Cmd>(command_tag<Cmd>) {
                    std::cout << '\t' << Cmd;
                },
                [&]<opcode Cmd>(command_tag<Cmd>, jump_address &addr) {
                    std::cout << '\t' << Cmd;
                    if (addr.label.empty()) {
                        auto it_label = line + addr.address;
                        if (it_label->command() == opcode::LABEL) {
                            addr.label = it_label->template get_args<opcode::LABEL>();
                        }
                    }
                    std::cout << ' ' << print_args(addr);
                },
                []<opcode Cmd>(command_tag<Cmd>, const auto &args) {
                    std::cout << '\t' << Cmd << ' ' << print_args(args);
                }
            }, *line);
            std::cout << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Errore sconosciuto" << std::endl;
        return 1;
    }

    return 0;
}