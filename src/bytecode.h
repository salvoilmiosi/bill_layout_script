#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <any>
#include <filesystem>

#include "layout.h"

#define OPCODES { \
    O(NOP),         /* no operation */ \
    O(RDBOX),       /* pdf_rect -- poppler.get_text -> content_stack */ \
    O(MVBOX),       /* byte index -- var_stack -> spacer[index] */ \
    O(CALL),        /* string fun_name, byte numargs -- var_stack * numargs -> fun_name -> var_stack */ \
    O(THROWERR),    /* var_stack -> throw */ \
    O(ADDWARNING),  /* var_stack -> warnings */ \
    O(PARSENUM),    /* var_stack -> parse_num -> var_stack */ \
    O(PARSEINT),    /* var_stack -> parse_int -> var_stack */ \
    O(EQ),          /* var_stack * 2 -> a == b -> var_stack */ \
    O(NEQ),         /* var_stack * 2 -> a != b -> var_stack */ \
    O(AND),         /* var_stack * 2 -> a && b -> var_stack */ \
    O(OR),          /* var_stack * 2 -> a == b -> var_stack */ \
    O(NEG),         /* var_stack -> -top -> var_stack */ \
    O(NOT),         /* var_stack -> !top -> var_stack */ \
    O(ADD),         /* var_stack * 2 -> a + b -> var_stack */ \
    O(SUB),         /* var_stack * 2 -> a - b -> var_stack */ \
    O(MUL),         /* var_stack * 2 -> a * b -> var_stack */ \
    O(DIV),         /* var_stack * 2 -> a / b -> var_stack */ \
    O(GT),          /* var_stack * 2 -> a > b -> var_stack */ \
    O(LT),          /* var_stack * 2 -> a < b -> var_stack */ \
    O(GEQ),         /* var_stack * 2 -> a >= b -> var_stack */ \
    O(LEQ),         /* var_stack * 2 -> a >= b -> var_stack */ \
    O(SELVAR),      /* string name, byte index -- (name, index, index) -> ref_stack */ \
    O(SELVARTOP),   /* string name -- var_stack -> (name, top, top) -> ref_stack */ \
    O(SELRANGETOP), /* string name -- var_stack * 2 -> (name, top-1, top) -> ref_stack */ \
    O(SELRANGEALL), /* string name -- (name, range_all) -> ref_stack */ \
    O(CLEAR),       /* ref_stack -> clear */ \
    O(SETVAR),      /* ref_stack, var_stack -> set */ \
    O(RESETVAR),    /* ref_stack, var_stack -> reset */ \
    O(PUSHVIEW),    /* content_stack -> var_stack */ \
    O(PUSHNUM),     /* fixed_point number -- number -> var_stack */ \
    O(PUSHSTR),     /* string str -- str -> var_stack */ \
    O(PUSHVAR),     /* ref_stack -> var_stack */ \
    O(MOVEVAR),     /* ref_stack -> (move) var_stack */ \
    O(JMP),         /* short address -- unconditional jump */ \
    O(JZ),          /* short address -- var_stack -> jump if top == 0 */ \
    O(JNZ),         /* short address -- var_stack -> jump if top != 0 */ \
    O(JTE),         /* short address -- jump if content_stack.top at token end */ \
    O(RET),         /* jump to return_addrs.top, return_addrs.pop, halt if return_addrs.empty */ \
    O(INC),         /* ref_stack, var_stack -> += top */ \
    O(DEC),         /* ref_stack, var_stack -> -= top */ \
    O(ISSET),       /* ref_stack -> size() != 0 -> var_stack */ \
    O(GETSIZE),     /* ref_stack -> size() -> var_stack */ \
    O(MOVCONTENT),  /* var_stack -> content_stack */ \
    O(SETBEGIN),    /* var_stack -> content_stack.top.setbegin */ \
    O(SETEND),      /* var_stack -> content_stack.top.setend */ \
    O(NEWVIEW),     /* content_stack.top.newview */ \
    O(NEWTOKENS),   /* content_stack.top.newtokens */ \
    O(RESETVIEW),   /* content_stack.top.resetview */ \
    O(NEXTLINE),    /* content_stack.top.next_token('\n') */ \
    O(NEXTTOKEN),   /* content_stack.top.next_token(' ') */ \
    O(POPCONTENT),  /* content_stack.pop() */ \
    O(NEXTTABLE),   /* current_table++ */ \
    O(ATE),         /* m_ate -> var_stack */ \
    O(IMPORT),      /* string layout_name, importa il file e lo esegue */ \
    O(SETLAYOUT),   /* string layout_name, IMPORT + hint per autolayout */ \
    O(COMMENT),     /* string data */ \
    O(SETLANG),     /* string language name */ \
}

#define O(x) x
enum class opcode : uint8_t OPCODES;
#undef O

typedef int8_t small_int;
struct command_call {
    std::string name;
    small_int numargs;
};

enum class spacer_index : uint8_t {
    SPACER_PAGE,
    SPACER_X,
    SPACER_Y,
    SPACER_W,
    SPACER_H,
};

struct variable_idx {
    std::string name;
    small_int index;
    small_int range_len;
};

struct jump_address {
    std::string label;
    int16_t address; // indirizzo relativo
};

class command_args {
private:
    opcode m_command;
    std::any m_data;

public:
    command_args(opcode command = opcode::NOP) : m_command(command) {}

    template<typename T>
    command_args(opcode command, T &&data) : m_command(command), m_data(std::forward<T>(data)) {}

    opcode command() const noexcept {
        return m_command;
    }
    
    template<typename T> T &get() {
        return *std::any_cast<T>(&m_data);
    }
    
    template<typename T> const T &get() const {
        return *std::any_cast<T>(&m_data);
    }

    const auto &type() const noexcept {
        return m_data.type();
    }
};

using bytecode = std::vector<command_args>;

std::ostream &operator << (std::ostream &out, const bytecode &code);

#endif