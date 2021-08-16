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

template<> std::ostream &operator << (std::ostream &out, const print_args<small_int> &args) {
    return out << int(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<readbox_options> &args) {
    return out << print_args(args->mode) << ' ' << print_args(args->flags);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_call> &args) {
    return out << args->fun->first << ' ' << print_args(args->numargs);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<fixed_point> &args) {
    return out << fixed_point_to_string(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<std::string> &args) {
    return out << unicode::escapeString(*args);
}

template<> std::ostream &operator << (std::ostream &out, const print_args<jump_address> &label) {
    if (!label.data.label.empty()) {
        return out << print_args(label.data.label);
    } else {
        return out << label.data.address;
    }
}

template<> std::ostream &operator << (std::ostream &out, const print_args<command_args> &line) {
    out << '\t' << print_args(line->command());
    line->visit([&](const auto &args) {
        if constexpr (! std::is_same_v<std::monostate, std::decay_t<decltype(args)>>) {
            out << ' ' << print_args(args);
        }
    });
    return out;
}

}

#endif