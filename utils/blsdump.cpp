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

        po::options_description desc(intl::format("OPTIONS"));
        po::positional_options_description pos;
        pos.add("input-bls", -1);

        desc.add_options()
            ("help,h", intl::format("PRINT_HELP").c_str())
            ("input-bls", po::value(&app.input_file), intl::format("BLS_INPUT_FILE").c_str())
            ("skipcomments,s", po::bool_switch(&app.skip_comments), intl::format("SKIP_COMMENTS").c_str())
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

template<typename T> struct print_args {
    const T &data;
    print_args(const T &data) : data(data) {}
};

template<typename T> print_args(T) -> print_args<T>;

template<typename T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
    return out << args.data;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<small_int> &args) {
    return out << int(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<readbox_options> &args) {
    return out << args.data.mode << ' ' << args.data.flags;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_call> &args) {
    return out << args.data.fun->first << ' ' << print_args(args.data.numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << fixed_point_to_string(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<std::string> &args) {
    return out << unicode::escapeString(args.data);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jump_label> &args) {
    return out << args.data;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jump_address> &label) {
    if (!label.data.label.empty()) {
        return out << print_args(label.data.label);
    } else {
        return out << label.data.address;
    }
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jsr_address> &args) {
    return out << print_args<jump_address>(args.data) << ' ' << print_args(args.data.numargs);
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
                line->visit([&]<typename T>(T &args) {
                    if constexpr (std::is_base_of_v<jump_address, T>) {
                        if (args.label.empty()) {
                            auto it_label = line + args.address;
                            if (it_label->command() == opcode::LABEL) {
                                args.label = it_label->template get_args<opcode::LABEL>();
                            }
                        }
                    }
                    if constexpr (! std::is_same_v<std::monostate, T>) {
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