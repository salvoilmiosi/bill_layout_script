#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <memory>

#include "layout.h"

enum class opcode : uint8_t {
    NOP,        // nessuna operazione
    RDBOX,      // byte mode, byte page, string spacers, float x, float y, float w, float h -- legge il rettangolo e resetta content_stack
    RDPAGE,     // byte mode, byte page, string spacers -- legge la pagina e resetta content_stack
    RDFILE,     // byte mode -- legge l'intero file e resetta content_stack
    MVBOX,      // byte index -- pop, sposta il rettangolo a seconda dello spacer
    SETPAGE,    // byte page -- setta la pagina letta
    CALL,       // string fun_name, byte numargs -- pop * numargs, push della variabile ritornata
    THROWERR,   // string message -- throw layout_error(message)
    PARSENUM,   // pop, push parse_num su top
    PARSEINT,   // pop, push parse_int su top
    EQ,         // pop * 2, push a == b
    NEQ,        // pop * 2, push a != b
    AND,        // pop * 2, push a && b
    OR,         // pop * 2, push a || b
    NEG,        // pop, push -a
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
    SELGLOBAL,  // string name -- m_selected = (name) VAR_GLOBAL
    SELVAR,     // string name, byte index -- m_selected = (name, index, index)
    SELVARTOP,  // string name -- pop, m_selected = (name, top)
    SELRANGE,   // string name, bye idxfrom, byte idxto = (name, idxfrom, idxto)
    SELRANGETOP,// string name -- pop, pop, m_selected = (name, top-1, top)
    SELRANGEALL,// string name -- m_selected = (name), set flag range_all
    SETDEBUG,   // setta flag debug su top di ref_stack
    CLEAR,      // m_selected.clear()
    APPEND,     // pop, m_selected.append(top)
    SETVAR,     // pop, m_selected = top
    RESETVAR,   // m_selected.clear(), setvar
    COPYCONTENT,// push copia top di content_stack su var_stack
    PUSHINT,    // byte number, push number su var_stack
    PUSHFLOAT,  // float number, push number su var_stack
    PUSHSTR,    // string str, push str su var_stack
    PUSHVAR,    // push m_selected su var_stack
    JMP,        // short address -- jump incondizionale
    JZ,         // short address -- pop, jump se top = 0
    JNZ,        // short address -- pop, jump se top != 0
    JTE,        // short address -- jump se top di content_stack a fine di token
    INCTOP,     // pop, m_selected += top
    INC,        // byte amount -- m_selected += amount
    DECTOP,     // pop, m_selected -= top
    DEC,        // m_selected -= amount
    ISSET,      // push m_selected.size() != 0
    GETSIZE,    // push m_selected.size()
    PUSHCONTENT,// pop, push top var_stack in content_stack
    NEXTLINE,   // avanza di un token newline nel top di content_stack
    NEXTTOKEN,  // avanza di un token spazio nel top di content_stack
    POPCONTENT, // pop da content_stack
    NEXTPAGE,   // m_page_num++
    ATE,        // push 1 se a fine file, 0 altrimenti
    STRDATA,    // short len, (byte * len) data -- dati stringa
    HLT=0xff,   // stop esecuzione
};

typedef int8_t byte_int;
typedef int16_t small_int;
typedef uint16_t string_ref;
typedef uint16_t jump_address;
typedef uint16_t string_size;

constexpr int32_t MAGIC = 0xb011377a;
constexpr int FLOAT_PRECISION = 10;

struct command_call {
    string_ref name;
    byte_int numargs;
};

enum spacer_index {
    SPACER_PAGE,
    SPACER_X,
    SPACER_Y,
    SPACER_W,
    SPACER_H
};

struct variable_idx {
    string_ref name;
    byte_int index_first;
    byte_int index_last;

    variable_idx() {}
    variable_idx(const string_ref &name, byte_int index) : name(name), index_first(index), index_last(index) {}
    variable_idx(const string_ref &name, byte_int index_first, byte_int index_last) : name(name), index_first(index_first), index_last(index_last) {}
};

struct command_args {
    const opcode command;

    const std::shared_ptr<void> data;

    command_args(opcode command = opcode::NOP) : command(command) {}

    template<typename T>
    command_args(opcode command, const T& data) : command(command), data(std::make_shared<T>(data)) {}
    
    template<typename T> const T &get() const {
        return *std::static_pointer_cast<T>(data);
    }
};

struct bytecode {
    std::vector<command_args> m_commands;
    std::vector<std::string> m_strings;

    std::ostream &write_bytecode(std::ostream &output);
    std::istream &read_bytecode(std::istream &input);
    
    string_ref add_string(const std::string &str);
    const std::string &get_string(string_ref ref);
};

#endif