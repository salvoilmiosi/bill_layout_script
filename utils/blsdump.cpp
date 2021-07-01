#include <iostream>
#include <filesystem>
#include <iomanip>

#include "cxxopts.hpp"
#include "parser.h"
#include "fixed_point.h"
#include "utils.h"
#include "binary_bls.h"

struct MainApp {
    int run();

    std::filesystem::path input_file;
    std::filesystem::path output_cache;

    bool skip_comments;
    bool recursive_imports;
    bool do_read_cache;
};

int main(int argc, char **argv) {
    MainApp app;

    try {
        cxxopts::Options options(argv[0], "Analyzes bls compiler output");

        options.positional_help("Input-bls-File");

        options.add_options()
            ("input-bls", "Input bls File", cxxopts::value(app.input_file))
            ("s,skip-comments", "Skip Comments", cxxopts::value(app.skip_comments))
            ("r,recursive-imports", "Recursive Imports", cxxopts::value(app.recursive_imports))
            ("c,read-cache", "Read Cache", cxxopts::value(app.do_read_cache))
            ("o,output-cache", "Output Cache File", cxxopts::value(app.output_cache))
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

template<typename T> struct print_args {
    const T &data;
    print_args(const T &data) : data(data) {}
};

template<typename T> print_args(T) -> print_args<T>;

template<typename T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
    return out << args.data;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<readbox_options> &args) {
    return out << args.data.mode << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_call> &args) {
    return out << args.data.fun->first << ' ' << num_tostring(args.data.numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<variable_selector> &args) {
    out << *args.data.name << ' ' << int(args.data.index);
    if (args.data.length != 1) {
        out << ':' << int(args.data.length);
    }
    return out << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << fixed_point_to_string(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<string_ptr> &args) {
    return out << std::quoted(*args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<small_int> &args) {
    return out << num_tostring(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jump_address> &label) {
    std::visit(overloaded{
        [&](string_ptr str) {
            out << print_args(str);
        },
        [&](ptrdiff_t addr) {
            out << addr;
        }
    }, label.data);
    return out;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jsr_address> &args) {
    return out << print_args(static_cast<jump_address>(args.data)) << ' ' << num_tostring(args.data.numargs);
}

int MainApp::run() {
    try {
        bytecode code;

        if (!do_read_cache) {
            parser my_parser;
            if (!skip_comments) my_parser.add_flag(parser_flags::ADD_COMMENTS);
            if (recursive_imports) my_parser.add_flag(parser_flags::RECURSIVE_IMPORTS);
            my_parser.read_layout(input_file, layout_box_list::from_file(input_file));
            code = std::move(my_parser).get_bytecode();
        } else {
            code = binary_bls::read(input_file);
        }

        if (!output_cache.empty()) {
            binary_bls::write(code, output_cache);
        }
        for (auto line = code.begin(); line != code.end(); ++line) {
            if (line->command() == opcode::COMMENT) {
                std::cout << *line->get_args<opcode::COMMENT>();
            } else {
                std::cout << '\t' << line->command();
                line->visit([&](auto args) {
                    if constexpr (std::is_base_of_v<jump_address, decltype(args)>) {
                        auto &addr = static_cast<jump_address &>(args);
                        if (auto *diff = std::get_if<ptrdiff_t>(&addr)) {
                            auto it_label = line + *diff;
                            if (it_label->command() == opcode::LABEL) {
                                addr = it_label->get_args<opcode::LABEL>();
                            }
                        }
                    }
                    if constexpr (! std::is_same_v<std::monostate, decltype(args)>) {
                        std::cout << ' ' << print_args(args);
                    }
                });
            }
            std::cout << std::endl;
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