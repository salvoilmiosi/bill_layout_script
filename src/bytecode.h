#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <any>
#include <typeinfo>
#include <cassert>

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "intl.h"
#include "functions.h"

#define OPCODES \
O(NOP)                          /* no operation */ \
O(SETBOX, pdf_rect)             /* imposta il box da leggere */ \
O(MVBOX, spacer_index)          /* stack -> current_box[index] */ \
O(RDBOX)                        /* poppler.get_text(current_box) -> content_stack */ \
O(NEXTTABLE)                    /* current_table++ */ \
O(SELVAR, variable_selector)    /* (name, index, size, flags) -> selected */ \
O(SETVAR, bitset<setvar_flags>) /* selected, stack -> set */ \
O(CLEAR)                        /* selected -> clear */ \
O(ISSET)                        /* selected -> size() != 0 -> stack */ \
O(GETSIZE)                      /* selected -> size() -> stack */ \
O(PUSHVAR)                      /* selected -> stack */ \
O(PUSHREF)                      /* selected.str_view -> stack */ \
O(PUSHVIEW)                     /* content_stack -> stack */ \
O(PUSHNUM, fixed_point)         /* number -> stack */ \
O(PUSHINT, int64_t)             /* int -> stack */ \
O(PUSHSTR, std::string)         /* str -> stack */ \
O(PUSHARG, small_int)           /* stack -> stack */ \
O(GETBOX, spacer_index)         /* current_box[index] -> stack */ \
O(DOCPAGES)                     /* m_doc.pages -> stack */ \
O(ATE)                          /* (getbox(page) >= docpages()) -> stack */ \
O(CALL, command_call)           /* stack * numargs -> fun_name -> stack */ \
O(ADDCONTENT)                   /* stack -> content_stack */ \
O(POPCONTENT)                   /* content_stack.pop() */ \
O(SETBEGIN)                     /* stack -> content_stack.top.setbegin */ \
O(SETEND)                       /* stack -> content_stack.top.setend */ \
O(NEWVIEW)                      /* content_stack.top.newview() */ \
O(SPLITVIEW)                    /* content_stack.top.splitview() */ \
O(NEXTRESULT)                   /* content_stack.top.nextresult() */ \
O(RESETVIEW)                    /* content_stack.top.resetview() */ \
O(THROWERROR)                   /* stack -> throw */ \
O(WARNING)                      /* stack -> warnings */ \
O(IMPORT, import_options)       /* importa il file e lo esegue */ \
O(SETLANG, intl::language)      /* imposta la lingua del layout */ \
O(JMP, jump_address)            /* unconditional jump */ \
O(JZ, jump_address)             /* stack -> jump if top == 0 */ \
O(JNZ, jump_address)            /* stack -> jump if top != 0 */ \
O(JNTE, jump_address)           /* jump if content_stack.top at token end */ \
O(JSR, jsr_address)             /* program_counter -> call_stack -- jump to subroutine and discard return value */ \
O(JSRVAL, jsr_address)          /* program_counter -> call_stack -- jump to subroutine */ \
O(UNEVAL_JUMP, jump_uneval)     /* unevaluated jump, sara' sostituito con opcode */ \
O(RET)                          /* jump to call_stack.top */ \
O(RETVAL)                       /* return to caller and push value to stack */ \
O(HLT)                          /* ferma l'esecuzione */

#define O_1_ARGS(x) O_IMPL(x, void)
#define O_2_ARGS(x, t) O_IMPL(x, t)

#define GET_3RD(arg1, arg2, arg3, ...) arg3
#define O_CHOOSER(...) GET_3RD(__VA_ARGS__, O_2_ARGS, O_1_ARGS)
#define O(...) O_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define O_IMPL(x, t) x,
enum class opcode : uint8_t { OPCODES };
#undef O_IMPL
constexpr const char *ToString(opcode op) {
#define O_IMPL(x, t) case opcode::x : return #x;
    switch (op) {
        OPCODES
        default: throw std::invalid_argument("[Unknown opcode]");
    }
#undef O_IMPL
}

typedef int16_t jump_address; // indirizzo relativo

struct command_call {
    function_iterator fun;
    small_int numargs;

    command_call(std::string_view name, int numargs) : fun(function_lookup.find(name)), numargs(numargs) {
        assert(fun != function_lookup.end());
    }
    command_call(function_iterator fun, int numargs) : fun(fun), numargs(numargs) {}
};

DEFINE_ENUM(spacer_index,
    (PAGE)
    (X)
    (Y)
    (WIDTH)
    (HEIGHT)
    (TOP)
    (RIGHT)
    (BOTTOM)
    (LEFT)
    (ROTATE_CW)
    (ROTATE_CCW)
)

DEFINE_ENUM_FLAGS(selvar_flags,
    (GLOBAL)
    (DYN_IDX)
    (DYN_LEN)
    (EACH)
    (APPEND)
)

struct variable_selector {
    std::string name;
    small_int index = 0;
    small_int length = 1;
    bitset<selvar_flags> flags;
};

DEFINE_ENUM_FLAGS(setvar_flags,
    (FORCE)
    (OVERWRITE)
    (INCREASE)
)

struct jump_uneval {
    std::string label;
    opcode cmd;
    std::any args;
};

struct jsr_address {
    jump_address addr;
    small_int numargs;
};

DEFINE_ENUM_FLAGS(import_flags,
    (NOIMPORT)
    (SETLAYOUT)
)

struct import_options {
    std::filesystem::path filename;
    bitset<import_flags> flags;
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

template<opcode Cmd, typename ... Ts>
command_args make_command(Ts && ... args) {
    return command_args(Cmd, opcode_type<Cmd>{ std::forward<Ts>(args) ... });
}

template<opcode Cmd> requires std::is_void_v<opcode_type<Cmd>>
command_args make_command() {
    return command_args(Cmd);
}

using bytecode = std::vector<command_args>;

#endif