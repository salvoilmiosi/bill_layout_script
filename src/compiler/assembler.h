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
    HLT=0xff,
};

struct command_call {
    std::string name;
    uint8_t numargs;
};

struct command_spacer {
    std::string name;
    float w;
    float h;
};

struct variable_idx {
    std::string name;
    uint8_t index;

    variable_idx() {}
    variable_idx(const std::string &name, uint8_t index) : name(name), index(index) {}
};

template<typename T> struct get_datasize { static constexpr int value = sizeof(T); };

template<> struct get_datasize<std::string> { static constexpr size_t value = 2; };
template<> struct get_datasize<command_call> { static constexpr size_t value = 3; };
template<> struct get_datasize<command_spacer> { static constexpr size_t value = 11; };
template<> struct get_datasize<variable_idx> { static constexpr size_t value = 3; };

struct command_args {
    const asm_command command;

    const std::shared_ptr<void> data;
    const size_t datasize = 0;

    command_args(asm_command command = NOP) : command(command) {}

    template<typename T>
    command_args(asm_command command, const T& data, size_t datasize = get_datasize<T>::value) :
        command(command), data(std::make_shared<T>(data)), datasize(datasize) {}
    
    template<typename T> const T &get() const {
        return *std::static_pointer_cast<T>(data);
    }
};

struct assembly_error {
    std::string message;
};

class assembler {
public:
    void read_lines(const std::vector<std::string> &lines);
    void save_output(std::ostream &output);

private:
    std::vector<command_args> out_lines;
    std::vector<std::pair<std::string, int>> out_strings;
};

#endif