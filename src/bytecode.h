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
O(SETVAR, flags_t)              /* selected, stack -> set(flags) */ \
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
O(JSR, jsr_address)             /* program_counter -> call_stack -- jump to subroutine */ \
O(JZ, jump_address)             /* stack -> jump if top == 0 */ \
O(JNZ, jump_address)            /* stack -> jump if top != 0 */ \
O(JNTE, jump_address)           /* jump if content_stack.top at token end */ \
O(UNEVAL_JUMP, jump_uneval)     /* unevaluated jump, sara' sostituito con opcode */ \
O(RET)                          /* jump to call_stack.top */ \
O(RETVAL)                       /* return to caller and push value to stack */ \
O(HLT)                          /* ferma l'esecuzione */

#define O_1_ARGS(x) O_IMPL(x, void)
#define O_2_ARGS(x, t) O_IMPL(x, t)

#define GET_3RD(arg1, arg2, arg3, ...) arg3
#define O_CHOOSER(...) GET_3RD(__VA_ARGS__, O_2_ARGS, O_1_ARGS)
#define O(...) O_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define O_IMPL(x, t) OP_##x,
enum opcode : uint8_t { OPCODES };
#undef O_IMPL

#define O_IMPL(x, t) #x,
static const char *opcode_names[] = { OPCODES };
#undef O_IMPL

typedef int16_t jump_address; // indirizzo relativo

struct command_call {
    function_iterator fun;
    small_int numargs;

    command_call(std::string_view name, int numargs) : fun(function_lookup.find(name)), numargs(numargs) {
        assert(fun != function_lookup.end());
    }
    command_call(function_iterator fun, int numargs) : fun(fun), numargs(numargs) {}
};

#define SPACER_INDICES \
F(PAGE) \
F(X) \
F(Y) \
F(WIDTH) \
F(HEIGHT) \
F(TOP) \
F(RIGHT) \
F(BOTTOM) \
F(LEFT) \
F(ROTATE_CW) \
F(ROTATE_CCW) \

#define F(x) SPACER_##x,
enum spacer_index : uint8_t { SPACER_INDICES };
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
enum selvar_flags : flags_t { SELVAR_FLAGS };
#undef F
#define F(x) #x,
static const char *selvar_flags_names[] = { SELVAR_FLAGS };
#undef F

struct variable_selector {
    std::string name;
    small_int index = 0;
    small_int length = 1;
    flags_t flags = 0;
};

#define SETVAR_FLAGS \
F(FORCE) \
F(OVERWRITE) \
F(INCREASE)

#define F(x) POS_SET_##x,
enum { SETVAR_FLAGS };
#undef F
#define F(x) SET_##x = (1 << POS_SET_##x),
enum setvar_flags : flags_t { SETVAR_FLAGS };
#undef F
#define F(x) #x,
static const char *setvar_flags_names[] = { SETVAR_FLAGS };
#undef F

struct jump_uneval {
    std::string label;
    opcode cmd;
    std::any args;
};

struct jsr_address {
    jump_address addr;
    small_int numargs;
    bool nodiscard;
};

#define IMPORT_FLAGS \
F(IGNORE) \
F(SETLAYOUT)

#define F(x) POS_IMPORT_##x,
enum { IMPORT_FLAGS };
#undef F
#define F(x) IMPORT_##x = (1 << POS_IMPORT_##x),
enum import_flags : flags_t { IMPORT_FLAGS };
#undef F
#define F(x) #x,
static const char *import_flags_names[] = { IMPORT_FLAGS };
#undef F

struct import_options {
    std::filesystem::path filename;
    flags_t flags;
};

#define O_IMPL(x, t) template<> struct opcode_type_impl<OP_##x> { using type = t; };
template<opcode Cmd> struct opcode_type_impl {};
template<opcode Cmd> using opcode_type = typename opcode_type_impl<Cmd>::type;
OPCODES
#undef O_IMPL

class command_args {
private:
    opcode m_command;
    std::any m_data;

    bool check_type() const {
#define O_IMPL(x, t) case OP_##x: return m_data.type() == typeid(opcode_type<OP_##x>);
        switch (m_command) {
            OPCODES
        }
        return false;
#undef O_IMPL
    }

public:
    command_args(opcode command = OP_NOP) : m_command(command) {
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