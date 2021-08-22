#ifndef __BYTECODE_PRINTER_H__
#define __BYTECODE_PRINTER_H__

#include "bytecode.h"
#include "unicode.h"
#include "enums.h"

namespace bls {

template<typename T> struct print_args {
    const T &data;
    print_args(const T &data) : data(data) {}

    const T &operator *() const {
        return data;
    }

    const T *operator ->() const {
        return &data;
    }
};

template<typename T> print_args(T) -> print_args<T>;

template<typename T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
    return out << *args;
}

template<enums::is_enum T> std::ostream &operator << (std::ostream &out, const print_args<T> &args) {
    return out << enums::to_string(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<readbox_options> &args) {
    return out << print_args(args->mode) << ' ' << print_args(args->flags);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_call> &args) {
    return out << args->fun->first << ' ' << print_args(args->numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << decimal_to_string(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<std::string> &args) {
    return out << unicode::escapeString(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_label> &label) {
    return out << "__" << label->id;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_node> &node) {
    if ((*node)->command() == opcode::LABEL) {
        out << print_args((*node)->template get_args<opcode::LABEL>());
    }
    return out;
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_args> &line) {
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
        }
    }, *line);
    return out;
}

}

#endif