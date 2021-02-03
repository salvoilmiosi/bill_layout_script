#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <any>
#include <typeinfo>

#include "fixed_point.h"
#include "layout.h"

typedef int8_t small_int;
struct command_call {
    std::string name;
    small_int numargs;
};

#define SPACER_INDICES { \
    SPACER(PAGE), \
    SPACER(X), \
    SPACER(Y), \
    SPACER(W), \
    SPACER(H) \
}

#define SPACER(x) SPACER_##x
enum class spacer_index : uint8_t SPACER_INDICES;
#undef SPACER
#define SPACER(x) #x
constexpr const char *spacer_index_names[] = SPACER_INDICES;
#undef SPACER

struct variable_name {
    std::string name;
    bool global;
};

struct variable_idx {
    variable_name name;
    small_int index;
    small_int range_len;
};

struct jump_address {
    std::string label;
    int16_t address; // indirizzo relativo
};

#define OPCODES { \
    O(NOP),                         /* no operation */ \
    O(RDBOX,        pdf_rect),      /* poppler.get_text -> content_stack */ \
    O(MVBOX,        spacer_index),  /* var_stack -> spacer[index] */ \
    O(CALL,         command_call),  /* var_stack * numargs -> fun_name -> var_stack */ \
    O(THROWERR),                    /* var_stack -> throw */ \
    O(ADDWARNING),                  /* var_stack -> warnings */ \
    O(PARSENUM),                    /* var_stack -> parse_num -> var_stack */ \
    O(PARSEINT),                    /* var_stack -> parse_int -> var_stack */ \
    O(EQ),                          /* var_stack * 2 -> a == b -> var_stack */ \
    O(NEQ),                         /* var_stack * 2 -> a != b -> var_stack */ \
    O(AND),                         /* var_stack * 2 -> a && b -> var_stack */ \
    O(OR),                          /* var_stack * 2 -> a == b -> var_stack */ \
    O(NEG),                         /* var_stack -> -top -> var_stack */ \
    O(NOT),                         /* var_stack -> !top -> var_stack */ \
    O(ADD),                         /* var_stack * 2 -> a + b -> var_stack */ \
    O(SUB),                         /* var_stack * 2 -> a - b -> var_stack */ \
    O(MUL),                         /* var_stack * 2 -> a * b -> var_stack */ \
    O(DIV),                         /* var_stack * 2 -> a / b -> var_stack */ \
    O(GT),                          /* var_stack * 2 -> a > b -> var_stack */ \
    O(LT),                          /* var_stack * 2 -> a < b -> var_stack */ \
    O(GEQ),                         /* var_stack * 2 -> a >= b -> var_stack */ \
    O(LEQ),                         /* var_stack * 2 -> a >= b -> var_stack */ \
    O(SELVAR,       variable_idx),  /* (name, index, size) -> ref_stack */ \
    O(SELVARTOP,    variable_name), /* var_stack -> (name, top, 1) -> ref_stack */ \
    O(SELRANGETOP,  variable_name), /* var_stack * 2 -> (name, top-1, top) -> ref_stack */ \
    O(SELRANGEALL,  variable_name), /* (name, 0, size) -> ref_stack */ \
    O(ISSET,        variable_name), /* ref_stack -> size() != 0 -> var_stack */ \
    O(GETSIZE,      variable_name), /* ref_stack -> size() -> var_stack */ \
    O(CLEAR),                       /* ref_stack -> clear */ \
    O(SETVAR),                      /* ref_stack, var_stack -> set */ \
    O(RESETVAR),                    /* ref_stack, var_stack -> reset */ \
    O(PUSHVIEW),                    /* content_stack -> var_stack */ \
    O(PUSHNUM,      fixed_point),   /* number -> var_stack */ \
    O(PUSHSTR,      std::string),   /* str -> var_stack */ \
    O(PUSHVAR),                     /* ref_stack -> var_stack */ \
    O(MOVEVAR),                     /* ref_stack -> (move) var_stack */ \
    O(JMP,          jump_address),  /* unconditional jump */ \
    O(JSR,          jump_address),  /* program_counter -> return_addrs -- jump to subroutine */ \
    O(JZ,           jump_address),  /* var_stack -> jump if top == 0 */ \
    O(JNZ,          jump_address),  /* var_stack -> jump if top != 0 */ \
    O(JTE,          jump_address),  /* jump if content_stack.top at token end */ \
    O(RET),                         /* jump to return_addrs.top, return_addrs.pop, halt if return_addrs.empty */ \
    O(HLT),                         /* ferma l'esecuzione */ \
    O(INC),                         /* ref_stack, var_stack -> += top */ \
    O(DEC),                         /* ref_stack, var_stack -> -= top */ \
    O(MOVCONTENT),                  /* var_stack -> content_stack */ \
    O(SETBEGIN),                    /* var_stack -> content_stack.top.setbegin */ \
    O(SETEND),                      /* var_stack -> content_stack.top.setend */ \
    O(NEWVIEW),                     /* content_stack.top.newview */ \
    O(NEWTOKENS),                   /* content_stack.top.newtokens */ \
    O(RESETVIEW),                   /* content_stack.top.resetview */ \
    O(NEXTLINE),                    /* content_stack.top.next_token('\n') */ \
    O(NEXTTOKEN),                   /* content_stack.top.next_token(' ') */ \
    O(POPCONTENT),                  /* content_stack.pop() */ \
    O(NEXTTABLE),                   /* current_table++ */ \
    O(ATE),                         /* m_ate -> var_stack */ \
    O(IMPORT,    std::filesystem::path), /* importa il file e lo esegue */ \
    O(SETLAYOUT, std::filesystem::path), /* IMPORT + hint per autolayout */ \
    O(COMMENT,      std::string),   /* dati ignorati */ \
    O(SETLANG,      int),           /* imposta la lingua del layout */ \
}

#define O_1_ARGS(x) O_IMPL(x, void)
#define O_2_ARGS(x, t) O_IMPL(x, t)

#define GET_3RD(arg1, arg2, arg3, ...) arg3
#define O_CHOOSER(...) GET_3RD(__VA_ARGS__, O_2_ARGS, O_1_ARGS)
#define O(...) O_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define O_IMPL(x, t) x
enum class opcode : uint8_t OPCODES;
#undef O_IMPL

#define O_IMPL(x, t) #x
static const char *opcode_names[] = OPCODES;
#undef O_IMPL

#define O_IMPL(x, t) &typeid(t)
static const std::type_info *opcode_types[] = OPCODES;
#undef O_IMPL
class command_args {
private:
    opcode m_command;
    std::any m_data;

    template<typename T>
    void check_type() const {
        if(typeid(T) != *opcode_types[int(m_command)]) {
            assert(false);
        }
    }

public:
    command_args(opcode command = opcode::NOP) : m_command(command) {
        check_type<void>();
    }

    template<typename T>
    command_args(opcode command, T &&data) : m_command(command), m_data(std::forward<T>(data)) {
        check_type<T>();
    }

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

#endif