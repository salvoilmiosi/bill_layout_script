#include <iostream>
#include <filesystem>

#include <boost/program_options.hpp>

#include "parser.h"
#include "fixed_point.h"
#include "utils.h"
#include "binary_bls.h"
#include "unicode.h"

struct MainApp {
    int run();

    std::filesystem::path input_file;
    std::filesystem::path output_cache;

    bool skip_comments;
    bool do_read_cache;
};

int main(int argc, char **argv) {
    try {
        MainApp app;

        namespace po = boost::program_options;

        po::options_description desc("Allowed options");
        po::positional_options_description pos;
        pos.add("input-bls", -1);

        desc.add_options()
            ("help,h", "Print Help")
            ("input-bls", po::value(&app.input_file), "Input bls File")
            ("skipcomments,s", po::bool_switch(&app.skip_comments), "Skip Comments")
            ("read-cache,c", po::bool_switch(&app.do_read_cache), "Read Cache")
            ("output-cache,o", po::value(&app.output_cache), "Output Cache File")
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
        std::cerr << "Errore nel parsing delle opzioni: " << e.what() << std::endl;
        return -1;
    }
}

using namespace bls;

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
    return out << args.data.fun->first << ' ' << int(args.data.numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<variable_selector> &args) {
    out << args.data.name << ' ' << int(args.data.index);
    if (args.data.length != 1) {
        out << ':' << int(args.data.length);
    }
    return out << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << fixed_point_to_string(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<std::string> &args) {
    return out << unicode::escapeString(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<small_int> &args) {
    return out << int(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jump_address> &label) {
    std::visit(util::overloaded{
        [&](const std::string &str) {
            out << print_args(str);
        },
        [&](ptrdiff_t addr) {
            out << addr;
        }
    }, label.data);
    return out;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jsr_address> &args) {
    return out << print_args(static_cast<jump_address>(args.data)) << ' ' << int(args.data.numargs);
}

int MainApp::run() {
    try {
        bytecode code;

        if (!do_read_cache) {
            parser my_parser;
            if (!skip_comments) my_parser.add_flag(parser_flags::ADD_COMMENTS);
            my_parser.read_layout(input_file, layout_box_list::from_file(input_file));
            code = std::move(my_parser).get_bytecode();
        } else {
            code = binary::read(input_file);
        }

        if (!output_cache.empty()) {
            binary::write(code, output_cache);
        }
        for (auto line = code.begin(); line != code.end(); ++line) {
            if (line->command() == opcode::COMMENT) {
                std::cout << line->get_args<opcode::COMMENT>();
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