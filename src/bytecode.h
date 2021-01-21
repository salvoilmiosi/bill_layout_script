#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <any>
#include <filesystem>

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
    ADDWARNING, // var_stack -> warnings
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
    SELVAR,     // string name, byte index -- (name, index, index) -> ref_stack
    SELVARTOP,  // string name -- var_stack -> (name, top, top) -> ref_stack
    SELRANGE,   // string name, bye idxfrom, byte idxto = (name, idxfrom, idxto) -> ref_stack
    SELRANGETOP,// string name -- var_stack * 2 -> (name, top-1, top) -> ref_stack
    SELRANGEALL,// string name -- (name, range_all) -> ref_stack
    CLEAR,      // ref_stack -> clear
    SETVAR,     // ref_stack, var_stack -> set
    RESETVAR,   // ref_stack, var_stack -> reset
    PUSHVIEW,   // content_stack -> var_stack
    PUSHBYTE,   // byte number -- number -> var_stack
    PUSHSHORT,  // byte*2 number -- number -> var_stack
    PUSHINT,    // byte*4 number -- number -> var_stack
    PUSHDECIMAL,// fixed_point number -- number -> var_stack
    PUSHSTR,    // string str -- str -> var_stack
    PUSHVAR,    // ref_stack -> var_stack
    MOVEVAR,    // ref_stack -> (move) var_stack
    JMP,        // short address -- unconditional jump
    JSR,        // short address -- program_counter -> return_addrs, jump
    JZ,         // short address -- var_stack -> jump if top == 0
    JNZ,        // short address -- var_stack -> jump if top != 0
    JTE,        // short address -- jump if content_stack.top at token end
    RET,        // jump to return_addrs.top, return_addrs.pop, halt if return_addrs.empty
    INC,        // ref_stack, var_stack -> += top
    DEC,        // ref_stack, var_stack -> -= top
    ISSET,      // ref_stack -> size() != 0 -> var_stack
    GETSIZE,    // ref_stack -> size() -> var_stack
    MOVCONTENT, // var_stack -> content_stack
    SETBEGIN,   // var_stack -> content_stack.top.setbegin
    SETEND,     // var_stack -> content_stack.top.setend
    NEWVIEW,    // content_stack.top.newview
    NEWTOKENS,  // content_stack.top.newtokens
    RESETVIEW,  // content_stack.top.resetview
    NEXTLINE,   // content_stack.top.next_token('\n')
    NEXTTOKEN,  // content_stack.top.next_token(' ')
    POPCONTENT, // content_stack.pop()
    NEXTTABLE,  // current_table++
    ATE,        // m_ate -> var_stack
    STRDATA=0xfe,// short len, (byte * len) data -- string data (strings are represented by 2 byte addresses)
    COMMENT=0xff,// short len, (byte * len) data -- string debug data
};

typedef uint8_t small_int;
typedef uint16_t string_ref;
typedef int16_t jump_address; // indirizzo relativo
typedef uint16_t string_size;

constexpr int32_t MAGIC = 0xb011377a;

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
    small_int index = 0;
    small_int range_len = 1;

    variable_idx() = default;
    variable_idx(const string_ref &name, small_int index, small_int range_len = 1) :
        name(name), index(index), range_len(range_len) {}
};

class command_args {
private:
    opcode m_command;
    std::any m_data;

public:
    command_args(opcode command = opcode::NOP) : m_command(command) {}

    template<typename T>
    command_args(opcode command, T &&data) : m_command(command), m_data(std::forward<T>(data)) {}

    opcode command() const {
        return m_command;
    }
    
    template<typename T> T get() const {
        return std::any_cast<T>(m_data);
    }
};

struct assembly_error : std::runtime_error {
    assembly_error(const std::string &message) : std::runtime_error(message) {}
};

struct bytecode {
    std::vector<command_args> m_commands;
    std::vector<std::string> m_strings;

    friend std::ostream &operator << (std::ostream &output, const bytecode &code);
    friend std::istream &operator >> (std::istream &input, bytecode &code);

    static bytecode from_file(const std::filesystem::path &filename);
    static bytecode from_lines(const std::vector<std::string> &lines);

    bool save_file(const std::filesystem::path &filename) {
        std::ofstream ofs(filename, std::ios::out | std::ios::binary);
        return ! (ofs << *this).fail();
    }
    
    template<typename ... Ts> command_args add_command(Ts && ... args) {
        return m_commands.emplace_back(std::forward<Ts>(args) ...);
    }
    
    string_ref add_string(const std::string &str);
    const std::string &get_string(string_ref ref);
};

std::ostream &operator << (std::ostream &output, const bytecode &code);
std::istream &operator >> (std::istream &input, bytecode &code);

#endif