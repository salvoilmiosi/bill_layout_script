#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>

enum asm_command {
    NOP,
    RDBOX,
    CALL,
    EQ,
    NEQ,
    AND,
    OR,
    NOT,
    ADD,
    SUB,
    MUL,
    DIV,
    GT,
    LT,
    GEQ,
    LEQ,
    MAX,
    MIN,
    SETGLOBAL,
    SETDEBUG,
    CLEAR,
    APPEND,
    SETVAR,
    PUSHCONTENT,
    PUSHNUM,
    PUSHSTR,
    PUSHGLOBAL,
    PUSHVAR,
    SETINDEX,
    JMP,
    JZ,
    JTE,
    INCTOP,
    INC,
    INCGTOP,
    INCG,
    DECTOP,
    DEC,
    DECGTOP,
    DECG,
    ISSET,
    SIZE,
    CONTENTVIEW,
    NEXTLINE,
    NEXTTOKEN,
    POPCONTENT,
    SPACER,
};

class asm_line {
public:
    asm_command command;
    size_t arg_num = 0;
    char *args_data = nullptr;
    size_t args_size = 0;

    asm_line(asm_command cmd = NOP) : command(cmd) {}

    template<typename ... Ts> asm_line(asm_command cmd, const Ts &... args);
};

template<typename T> constexpr size_t get_size(const T &) {
    return sizeof(T);
}

template<> inline size_t get_size<> (const std::string &str) {
    return sizeof(short) + str.size();
}

constexpr size_t get_size_all() {
    return 0;
}

template<typename T, typename ... Ts>
constexpr size_t get_size_all(const T &first, const Ts & ... others) {
    return get_size(first) + get_size_all(others ...);
}

template<typename T>
constexpr size_t add_arg(char *out, const T &data) {
    memcpy(out, &data, sizeof(data));
    return sizeof(data);
}

template<> inline size_t add_arg(char *out, const std::string &data) {
    size_t bytes = add_arg(out, (short) data.size());
    memcpy(out + bytes, data.data(), data.size());
    return get_size(data);
}

constexpr void add_args(char *) {}

template<typename T, typename ... Ts>
void add_args(char *out, const T &first, const Ts & ... others) {
    size_t bytes = add_arg(out, first);
    add_args(out + bytes, others ...);
}

template<typename ... Ts>
asm_line::asm_line(asm_command cmd, const Ts &... args) : command(cmd) {
    arg_num = sizeof...(args);
    if (arg_num > 0) {
        args_size = get_size_all(args ...);
        args_data = new char[args_size];
        add_args(args_data, args ...);
    }
}

struct assembly_error {
    std::string message;
};

class assembler {
public:
    assembler(const std::vector<std::string> &lines);

    void save_output(std::ostream &output);

private:
    std::vector<asm_line> out_lines;
};

#endif