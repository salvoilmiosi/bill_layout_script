#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>
#include <memory>

#include "../shared/layout.h"

enum asm_command {
    NOP,
    RDBOX,
    CALL,
    ERROR,
    PARSENUM,
    PARSEINT,
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
    RESETVAR,
    PUSHCONTENT,
    PUSHNUM,
    PUSHSTR,
    PUSHGLOBAL,
    PUSHVAR,
    SETINDEX,
    SETINDEXTOP,
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
    NEXTCONTENT,
    NEXTLINE,
    NEXTTOKEN,
    POPCONTENT,
    SPACER,
    STRDATA,
    HLT=0xffffffff,
};

struct command_call {
    std::string name;
    int numargs;
};

struct command_spacer {
    std::string name;
    float w;
    float h;
};

template<typename T> struct get_datasize { static constexpr size_t value = sizeof(T); };

template<> struct get_datasize<std::string> { static constexpr size_t value = sizeof(int); };
template<> struct get_datasize<layout_box> { static constexpr size_t value = sizeof(int) * 8; };
template<> struct get_datasize<command_call> { static constexpr size_t value = sizeof(int) * 2; };
template<> struct get_datasize<command_spacer> { static constexpr size_t value = sizeof(int) * 3; };

struct command_args {
    asm_command command;

    std::shared_ptr<void> data;
    size_t datasize = 0;

    command_args(asm_command command = NOP) : command(command) {}

    template<typename T>
    command_args(asm_command command, std::shared_ptr<T> data) :
        command(command), data(data), datasize(get_datasize<T>::value) {}
};

struct assembly_error {
    std::string message;
};

class assembler {
public:
    assembler(const std::vector<std::string> &lines);

    void save_output(std::ostream &output);

private:
    std::vector<command_args> out_lines;
    std::vector<std::pair<std::string, int>> out_strings;
};

#endif