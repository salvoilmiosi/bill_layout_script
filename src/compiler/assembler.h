#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>

enum asm_command {
    NOP,
    RDBOX,
    CALL,
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

struct asm_line {
    asm_command command = NOP;
    size_t arg_num = 0;
    char *args_data = nullptr;
    size_t args_size = 0;

    asm_line() {}
    asm_line(asm_command cmd) : command(cmd) {}
};

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