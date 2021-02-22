#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <any>
#include <typeinfo>
#include <cassert>

#include "fixed_point.h"
#include "layout.h"

#define OPCODES \
O(NOP)                         /* no operation */ \
O(RDBOX,        pdf_rect)      /* poppler.get_text -> content_stack */ \
O(MVBOX,        spacer_index)  /* var_stack -> spacer[index] */ \
O(CALL,         command_call)  /* var_stack * numargs -> fun_name -> var_stack */ \
O(THROWERR)                    /* var_stack -> throw */ \
O(ADDWARNING)                  /* var_stack -> warnings */ \
O(PARSENUM)                    /* var_stack -> parse_num -> var_stack */ \
O(PARSEINT)                    /* var_stack -> parse_int -> var_stack */ \
O(AGGREGATE)                   /* var_stack -> split -> parse_int -> sum -> var_stack */ \
O(EQ)                          /* var_stack * 2 -> a == b -> var_stack */ \
O(NEQ)                         /* var_stack * 2 -> a != b -> var_stack */ \
O(AND)                         /* var_stack * 2 -> a && b -> var_stack */ \
O(OR)                          /* var_stack * 2 -> a == b -> var_stack */ \
O(NEG)                         /* var_stack -> -top -> var_stack */ \
O(NOT)                         /* var_stack -> !top -> var_stack */ \
O(ADD)                         /* var_stack * 2 -> a + b -> var_stack */ \
O(SUB)                         /* var_stack * 2 -> a - b -> var_stack */ \
O(MUL)                         /* var_stack * 2 -> a * b -> var_stack */ \
O(DIV)                         /* var_stack * 2 -> a / b -> var_stack */ \
O(GT)                          /* var_stack * 2 -> a > b -> var_stack */ \
O(LT)                          /* var_stack * 2 -> a < b -> var_stack */ \
O(GEQ)                         /* var_stack * 2 -> a >= b -> var_stack */ \
O(LEQ)                         /* var_stack * 2 -> a >= b -> var_stack */ \
O(SELVAR,   variable_selector) /* (name, index, size, flags) -> ref_stack */ \
O(ISSET)                       /* ref_stack -> size() != 0 -> var_stack */ \
O(GETSIZE)                     /* ref_stack -> size() -> var_stack */ \
O(CLEAR)                       /* ref_stack -> clear */ \
O(SETVAR,       uint8_t)       /* ref_stack, var_stack -> set(flags) */ \
O(PUSHVIEW)                    /* content_stack -> var_stack */ \
O(PUSHNUM,      fixed_point)   /* number -> var_stack */ \
O(PUSHSTR,      std::string)   /* str -> var_stack */ \
O(PUSHVAR)                     /* ref_stack -> var_stack */ \
O(PUSHNULL)                    /* null -> var_stack */ \
O(MOVEVAR)                     /* ref_stack -> (move) var_stack */ \
O(UNEVAL_JUMP,  jump_uneval)   /* unevaluated jump, sara' sostituito con opcode */ \
O(JMP,          jump_address)  /* unconditional jump */ \
O(JSR,          jump_address)  /* program_counter -> return_addrs -- jump to subroutine */ \
O(JZ,           jump_address)  /* var_stack -> jump if top == 0 */ \
O(JNZ,          jump_address)  /* var_stack -> jump if top != 0 */ \
O(JTE,          jump_address)  /* jump if content_stack.top at token end */ \
O(RET)                         /* jump to return_addrs.top, return_addrs.pop, halt if return_addrs.empty */ \
O(HLT)                         /* ferma l'esecuzione */ \
O(MOVCONTENT)                  /* var_stack -> content_stack */ \
O(SETBEGIN)                    /* var_stack -> content_stack.top.setbegin */ \
O(SETEND)                      /* var_stack -> content_stack.top.setend */ \
O(NEWVIEW)                     /* content_stack.top.newview() */ \
O(SUBVIEW)                     /* content_stack.top.newsubview() */ \
O(RESETVIEW)                   /* content_stack.top.resetview() */ \
O(NEXTRESULT)                  /* content_stack.top.next_result() */ \
O(POPCONTENT)                  /* content_stack.pop() */ \
O(NEXTTABLE)                   /* current_table++ */ \
O(ATE)                         /* m_ate -> var_stack */ \
O(IMPORT,    std::filesystem::path) /* importa il file e lo esegue */ \
O(SETLAYOUT, std::filesystem::path) /* IMPORT + hint per autolayout */ \
O(COMMENT, std::string)        /* dati ignorati */ \
O(SETLANG, intl::language) /* imposta la lingua del layout */

#define O_1_ARGS(x) O_IMPL(x, void)
#define O_2_ARGS(x, t) O_IMPL(x, t)

#define GET_3RD(arg1, arg2, arg3, ...) arg3
#define O_CHOOSER(...) GET_3RD(__VA_ARGS__, O_2_ARGS, O_1_ARGS)
#define O(...) O_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define O_IMPL(x, t) x,
enum class opcode : uint8_t { OPCODES };
#undef O_IMPL

#define O_IMPL(x, t) #x,
static const char *opcode_names[] = { OPCODES };
#undef O_IMPL
typedef int8_t small_int;
struct command_call {
    std::string name;
    small_int numargs;
};

#define SPACER_INDICES \
F(PAGE) \
F(X) \
F(Y) \
F(W) \
F(H) \
F(TOP) \
F(RIGHT) \
F(BOTTOM) \
F(LEFT)

#define F(x) SPACER_##x,
enum class spacer_index : uint8_t { SPACER_INDICES };
#undef F
#define F(x) #x,
constexpr const char *spacer_index_names[] = { SPACER_INDICES };
#undef F

#define SELVAR_FLAGS \
F(GLOBAL) \
F(DYN_IDX) \
F(DYN_LEN) \
F(EACH) \
F(APPEND)

#define F(x) POS_SEL_##x,
enum { SELVAR_FLAGS };
#undef F
#define F(x) SEL_##x = (1 << POS_SEL_##x),
enum selvar_flags { SELVAR_FLAGS };
#undef F
#define F(x) #x,
static const char *selvar_flags_names[] = { SELVAR_FLAGS };
#undef F

struct variable_selector {
    std::string name;
    small_int index = 0;
    small_int length = 1;
    uint8_t flags = 0;
};

#define SETVAR_FLAGS \
F(FORCE) \
F(OVERWRITE) \
F(INCREASE) \
F(DECREASE)

#define F(x) POS_SET_##x,
enum { SETVAR_FLAGS };
#undef F
#define F(x) SET_##x = (1 << POS_SET_##x),
enum setvar_flags { SETVAR_FLAGS };
#undef F
#define F(x) #x,
static const char *setvar_flags_names[] = { SETVAR_FLAGS };
#undef F

typedef int16_t jump_address; // indirizzo relativo

struct jump_uneval {
    opcode cmd;
    std::string label;
};

#define O_IMPL(x, t) template<> struct opcode_type_impl<opcode::x> { using type = t; };
template<opcode Cmd> struct opcode_type_impl {};
template<opcode Cmd> using opcode_type = typename opcode_type_impl<Cmd>::type;
OPCODES
#undef O_IMPL

class command_args {
private:
    opcode m_command;
    std::any m_data;

    bool check_type() const {
#define O_IMPL(x, t) case opcode::x: return m_data.type() == typeid(opcode_type<opcode::x>);
        switch (m_command) {
            OPCODES
        }
        return false;
#undef O_IMPL
    }

public:
    command_args(opcode command = opcode::NOP) : m_command(command) {
        assert(check_type());
    }

    template<typename T>
    command_args(opcode command, T &&data) : m_command(command), m_data(std::forward<T>(data)) {
        assert(check_type());
    }

    opcode command() const noexcept {
        return m_command;
    }
    
    template<opcode Cmd> const auto &get_args() const {
        return *std::any_cast<opcode_type<Cmd>>(&m_data);
    }
};

using bytecode = std::vector<command_args>;

#endif