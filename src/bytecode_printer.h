#ifndef __BYTECODE_PRINTER_H__
#define __BYTECODE_PRINTER_H__

#include "bytecode.h"
#include "utils/unicode.h"

namespace {
    using namespace enums::stream_operators;

    struct arg_printer {
        std::ostream &out;
        
        template<typename T> std::ostream &operator()(const T &value) {
            return out << value;
        }

        std::ostream &operator()(const std::string &str) {
            return out << unicode::escapeString(str);
        }

        std::ostream &operator()(bool value) {
            return out << std::boolalpha << value << std::noboolalpha;
        }

        std::ostream &operator()(bls::fixed_point num) {
            return out << bls::decimal_to_string(num);
        }

        std::ostream &operator()(bls::command_call call) {
            return out << call->first;
        }

        std::ostream &operator()(bls::command_label label) {
            return out << "__" << label.id;
        }
    };
}

namespace bls {
    template<typename T> struct print_args {
        const T &data;
        print_args(const T &data) : data(data) {}
    };
    template<typename T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
        return ::arg_printer{out}(args.data);
    }

    struct bytecode_printer {
        const command_list &list;
        command_node node;

        bytecode_printer(const command_list &list, command_node node)
            : list(list), node(node) {}
    };

    static std::ostream &operator << (std::ostream &out, const bytecode_printer &line) {
        visit_command(util::overloaded{
            [&](command_tag<opcode::LABEL>, const command_label &line) {
                out << print_args(line) << ':';
            },
            [&](command_tag<opcode::BOXNAME>, const std::string &line) {
                out << "### " << line;
            },
            [&](command_tag<opcode::COMMENT>, const std::string &line) {
                out << line;
            },
            [&]<opcode Cmd>(command_tag<Cmd>) {
                out << '\t' << print_args(Cmd);
            },
            [&]<opcode Cmd>(command_tag<Cmd>, const auto &args) {
                out << '\t' << print_args(Cmd) << ' ' << print_args(args);
            },
            [&]<opcode Cmd>(command_tag<Cmd>, const command_node &addr) {
                out << '\t' << print_args(Cmd) << ' ';
                if (addr->command() == opcode::LABEL) {
                    out << print_args(addr->get_args<opcode::LABEL>());
                } else {
                    out << (std::ranges::distance(line.list.begin(), addr)
                        - std::ranges::distance(line.list.begin(), line.node));
                }
            }
        }, *line.node);
        return out;
    }

}

#endif