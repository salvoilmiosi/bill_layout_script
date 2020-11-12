#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>
#include <memory>

#include "../shared/layout.h"

enum asm_command {
    NOP,
    RDBOX,      // byte mode, byte page, string spacers, float x, float y, float w, float h -- legge il rettangolo e resetta content_stack
    RDPAGE,     // byte mode, byte page, string spacers -- legge la pagina e resetta content_stack
    RDFILE,     // byte mode -- legge l'intero file e resetta content_stack
    CALL,       // string fun_name, byte numargs -- pop * numargs, push della variabile ritornata
    ERROR,      // string message -- throw layout_error(error)
    PARSENUM,   // pop, push parse_num su top
    PARSEINT,   // pop, push parse_int su top
    EQ,         // pop * 2, push a == b
    NEQ,        // pop * 2, push a != b
    AND,        // pop * 2, push a && b
    OR,         // pop * 2, push a || b
    NOT,        // pop, push !a
    ADD,        // pop * 2, push a + b
    SUB,        // pop * 2, push a - b
    MUL,        // pop * 2, push a * b
    DIV,        // pop * 2, push a / b
    GT,         // pop * 2, push a > b
    LT,         // pop * 2, push a < b
    GEQ,        // pop * 2, push a >= b
    LEQ,        // pop * 2, push a <= b
    MAX,        // pop * 2, push max (a, b)
    MIN,        // pop * 2, push min (a, b)
    SETGLOBAL,  // string name -- pop, aggiunge m_globals[name] = top
    SETDEBUG,   // setta flag debug su top
    CLEAR,      // string name -- cancella variabile name
    APPEND,     // string name -- pop, variables[name].append(top)
    SETVAR,     // string name -- pop * 2, variables[name][top - 1] = top
    SETVARIDX,  // string name, byte index -- pop, variables[name][index] = top
    RESETVAR,   // string name -- pop, variables[name] = {top}
    COPYCONTENT,// push top di content_stack su var_stack
    PUSHNUM,    // float number, push number su var_stack
    PUSHSTR,    // string str, push str su var_stack
    PUSHGLOBAL, // string name -- push m_globals[name] su var_stack
    PUSHVAR,    // string name -- pop, push variables[name][top] su var_stack
    PUSHVARIDX, // string name, byte index -- push variables[name][index] su var_stack
    JMP,        // short address -- jump incondizionale
    JZ,         // short address -- pop, jump se top = 0
    JTE,        // short address -- jump se top di content_stack a fine di token
    INCTOP,     // string name -- pop * 2, variables[name][top - 1] += top
    INC,        // string name -- pop, variables[name][top] += 1
    INCTOPIDX,  // string name, byte index -- pop, variables[name][index] += top
    INCIDX,     // string name, byte index -- variables[name][index] += 1
    INCGTOP,    // string name -- pop, m_globals[name] += top
    INCG,       // string name -- m_globals[name] += 1
    DECTOP,     // string name -- pop * 2, variables[name][top - 1] -= top
    DEC,        // string name -- pop, variables[name][top] -= 1
    DECTOPIDX,  // string name, byte index -- pop, variables[name][index] -= top
    DECIDX,     // string name, byte index -- variables[name][index] -= 1
    DECGTOP,    // string name -- pop, m_globals[name] -= top
    DECG,       // string name -- m_globals[name] -= 1
    ISSET,      // string name -- push isset(variables[name])
    SIZE,       // string name -- push size(variables[name])
    PUSHCONTENT,// push copia di top content_stack in content_stack
    NEXTLINE,   // avanza di un token newline nel top di content_stack
    NEXTTOKEN,  // avanza di un token spazio nel top di content_stack
    POPCONTENT, // pop da content_stack
    SPACER,     // string name, float w, float h -- m_globals[name] = {w, h}
    NEXTPAGE,   // m_page_num++
    STRDATA,    // short len, (byte * len) data -- dati stringa
    HLT=0xff,
};

typedef uint8_t command_int;
typedef uint8_t small_int;
typedef uint16_t string_ref;
typedef uint16_t jump_address;
typedef uint16_t string_size;

struct command_box : public layout_box {
    string_ref ref_spacers;
};

struct command_call {
    string_ref name;
    small_int numargs;
};

struct command_spacer {
    string_ref name;
    float w;
    float h;
};

struct variable_idx {
    string_ref name;
    small_int index;

    variable_idx() {}
    variable_idx(const string_ref &name, small_int index) : name(name), index(index) {}
};

struct command_args {
    const asm_command command;

    const std::shared_ptr<void> data;

    command_args(asm_command command = NOP) : command(command) {}

    template<typename T>
    command_args(asm_command command, const T& data) : command(command), data(std::make_shared<T>(data)) {}
    
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
    std::vector<std::string> out_strings;

    template<typename ... Ts> command_args add_command(const Ts & ... args) {
        return out_lines.emplace_back(args ...);
    }

    string_ref add_string(const std::string &str);
};

#endif