#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <memory>

#include "layout.h"

enum class opcode : uint8_t {
    NOP=0x00,   // no operation
    RDBOX,      // byte mode, byte page, string spacers, float x, float y, float w, float h -- pdftotext -> content_stack
    RDPAGE,     // byte mode, byte page, string spacers -- pdftotext -> content_stack
    RDFILE,     // byte mode -- pdftotext -> content_stack
    MVBOX,      // byte index -- var_stack -> spacer[index]
    SETPAGE,    // byte page -- m_ate = page > num pdf pages
    CALL,       // string fun_name, byte numargs -- var_stack * numargs -> fun_name -> var_stack
    THROWERR,   // var_stack -> throw
    PARSENUM,   // var_stack -> parse_num -> var_stack
    PARSEINT,   // var_stack -> parse_int -> var_stack
    EQ,         // var_stack * 2 -> a == b -> var_stack
    NEQ,        // var_stack * 2 -> a != b -> var_stack
    AND,        // var_stack * 2 -> a && b -> var_stack
    OR,         // var_stack * 2 -> a == b -> var_stack
    NEG,        // var_stack -> -top -> var_stack
    NOT,        // var_stack -> !top -> var_stack
    ADD,        // var_stack * 2 -> a + b -> var_stack
    SUB,        // var_stack * 2 -> a - b -> var_stack
    MUL,        // var_stack * 2 -> a * b -> var_stack
    DIV,        // var_stack * 2 -> a / b -> var_stack
    GT,         // var_stack * 2 -> a > b -> var_stack
    LT,         // var_stack * 2 -> a < b -> var_stack
    GEQ,        // var_stack * 2 -> a >= b -> var_stack
    LEQ,        // var_stack * 2 -> a >= b -> var_stack
    MAX,        // var_stack * 2 -> max(a,b) -> var_stack
    MIN,        // var_stack * 2 -> min(a,b) -> var_stack
    SELGLOBAL,  // string name -- (name, global) -> ref_stack
    SELVAR,     // string name, byte index -- (name, index, index) -> ref_stack
    SELVARTOP,  // string name -- var_stack -> (name, top, top) -> ref_stack
    SELRANGE,   // string name, bye idxfrom, byte idxto = (name, idxfrom, idxto) -> ref_stack
    SELRANGETOP,// string name -- var_stack * 2 -> (name, top-1, top) -> ref_stack
    SELRANGEALL,// string name -- (name, range_all) -> ref_stack
    SETDEBUG,   // ref_stack.top.debug = 1
    CLEAR,      // ref_stack -> clear
    APPEND,     // ref_stack, var_stack -> append
    SETVAR,     // ref_stack, var_stack -> set
    RESETVAR,   // ref_stack, var_stack -> reset
    COPYCONTENT,// content_stack (copy) -> var_stack
    PUSHBYTE,   // byte number -- number -> var_stack
    PUSHSHORT,  // byte*2 number -- number -> var_stack
    PUSHINT,    // byte*4 number -- number -> var_stack
    PUSHFLOAT,  // float number -- number -> var_stack
    PUSHSTR,    // string str -- str -> var_stack
    PUSHVAR,    // ref_stack -> var_stack
    JMP,        // short address -- unconditional jump
    JZ,         // short address -- var_stack -> jump if top == 0
    JNZ,        // short address -- var_stack -> jump if top != 0
    JTE,        // short address -- jump if content_stack.top at token end
    INC,        // ref_stack, var_stack -> += top
    DEC,        // ref_stack, var_stack -> -= top
    ISSET,      // ref_stack -> size() != 0 -> var_stack
    GETSIZE,    // ref_stack -> size() -> var_stack
    PUSHCONTENT,// var_stack -> content_stack
    NEXTLINE,   // content_stack.top.next_token('/n')
    NEXTTOKEN,  // content_stack.top.next_token(' ')
    POPCONTENT, // content_stack.pop()
    NEXTPAGE,   // m_page_num++
    ATE,        // m_ate -> var_stack
    STRDATA=0xfd,// short len, (byte * len) data -- string data (strings are represented by 2 byte addresses)
    DBGDATA=0xfe,// short len, (byte * len) data -- string debug data
    HLT=0xff,   // halt execution
};

typedef uint8_t small_int;
typedef uint16_t string_ref;
typedef uint16_t jump_address;
typedef uint16_t string_size;

constexpr int32_t MAGIC = 0xb011377a;
constexpr int FLOAT_PRECISION = 10;

struct command_call {
    string_ref name;
    small_int numargs;
};

enum class spacer_index: uint8_t {
    SPACER_PAGE,
    SPACER_X,
    SPACER_Y,
    SPACER_W,
    SPACER_H
};

struct variable_idx {
    string_ref name;
    small_int index_first;
    small_int index_last;

    variable_idx() = default;
    variable_idx(const string_ref &name, small_int index) : name(name), index_first(index), index_last(index) {}
    variable_idx(const string_ref &name, small_int index_first, small_int index_last) : name(name), index_first(index_first), index_last(index_last) {}
};

struct command_args {
    const opcode command;

    const std::shared_ptr<void> data;

    command_args(opcode command = opcode::NOP) : command(command) {}

    template<typename T>
    command_args(opcode command, const T &data) : command(command), data(std::make_shared<T>(data)) {}
    
    template<typename T> const T &get() const {
        return *std::static_pointer_cast<T>(data);
    }
};

struct bytecode {
    std::vector<command_args> m_commands;
    std::vector<std::string> m_strings;

    std::ostream &write_bytecode(std::ostream &output);
    std::istream &read_bytecode(std::istream &input);
    
    template<typename ... Ts> command_args add_command(Ts && ... args) {
        return m_commands.emplace_back(std::forward<Ts>(args) ...);
    }
    
    string_ref add_string(const std::string &str);
    const std::string &get_string(string_ref ref);
};

#endif