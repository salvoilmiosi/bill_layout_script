#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>
#include <memory>

enum asm_command {
    NOP,
    RDBOX,
    RDPAGE,
    RDFILE,
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
    SETVARIDX,
    RESETVAR,
    COPYCONTENT,
    PUSHNUM,
    PUSHSTR,
    PUSHGLOBAL,
    PUSHVAR,
    PUSHVARIDX,
    JMP,
    JZ,
    JTE,
    INCTOP,
    INC,
    INCTOPIDX,
    INCIDX,
    INCGTOP,
    INCG,
    DECTOP,
    DEC,
    DECTOPIDX,
    DECIDX,
    DECGTOP,
    DECG,
    ISSET,
    SIZE,
    PUSHCONTENT,
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

struct variable_idx {
    std::string name;
    int index;

    variable_idx() {}
    variable_idx(const std::string &name, int index) : name(name), index(index) {}
    variable_idx(const std::string &name, size_t index) : name(name), index(index) {}
};

template<typename T> struct get_datasize { static constexpr int value = sizeof(T); };

template<> struct get_datasize<std::string> { static constexpr size_t value = sizeof(int); };
template<> struct get_datasize<command_call> { static constexpr size_t value = sizeof(int) * 2; };
template<> struct get_datasize<command_spacer> { static constexpr size_t value = sizeof(int) * 3; };
template<> struct get_datasize<variable_idx> { static constexpr size_t value = sizeof(int) * 2; };

struct command_args {
    asm_command command;

    std::shared_ptr<void> data;
    size_t datasize = 0;

    command_args(asm_command command = NOP) : command(command) {}

    template<typename T>
    command_args(asm_command command, std::shared_ptr<T> data, size_t datasize = get_datasize<T>::value) :
        command(command), data(data), datasize(datasize) {}
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