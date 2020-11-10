#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>
#include <memory>

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
    HLT=0xffffffff,
};

struct command_args {
    asm_command command;

    std::shared_ptr<void> data;
    size_t datasize = 0;

    command_args(asm_command command = NOP) : command(command) {}
    command_args(asm_command command, std::shared_ptr<void> data, size_t datasize = 0) :
        command(command), data(data), datasize(datasize) {}
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

struct assembly_error {
    std::string message;
};

class assembler {
public:
    assembler(const std::vector<std::string> &lines);

    void save_output(std::ostream &output);

private:
    std::vector<command_args> out_lines;
};

#endif