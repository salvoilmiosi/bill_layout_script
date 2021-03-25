#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "intl.h"
#include "functions.h"

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

struct jump_address {
    int16_t relative_addr;
    std::string label;

    jump_address() = default;
    jump_address(std::string_view label)
        : relative_addr(0), label(label) {}
    jump_address(int16_t relative_addr)
        : relative_addr(relative_addr) {}
};

struct jsr_address : jump_address {
    small_int numargs;

    jsr_address() = default;
    jsr_address(auto &&args, small_int numargs)
        : jump_address(std::forward<decltype(args)>(args)), numargs(numargs) {}
};

DEFINE_ENUM_FLAGS(import_flags,
    (NOIMPORT)
    (SETLAYOUT)
)

struct import_options {
    std::filesystem::path filename;
    bitset<import_flags> flags;
};

DEFINE_ENUM_TYPES(opcode,
    (NOP)                          /* no operation */
    (SETBOX, pdf_rect)             /* imposta il box da leggere */
    (MVBOX, spacer_index)          /* stack -> current_box[index] */
    (RDBOX)                        /* poppler.get_text(current_box) -> content_stack */
    (NEXTTABLE)                    /* current_table++ */
    (SELVAR, variable_selector)    /* (name, index, size, flags) -> selected */
    (SETVAR, bitset<setvar_flags>) /* selected, stack -> set */
    (CLEAR)                        /* selected -> clear */
    (ISSET)                        /* selected -> size() != 0 -> stack */
    (GETSIZE)                      /* selected -> size() -> stack */
    (PUSHVAR)                      /* selected -> stack */
    (PUSHREF)                      /* selected.str_view -> stack */
    (PUSHVIEW)                     /* content_stack -> stack */
    (PUSHNUM, fixed_point)         /* number -> stack */
    (PUSHINT, int64_t)             /* int -> stack */
    (PUSHSTR, std::string)         /* str -> stack */
    (PUSHARG, small_int)           /* stack -> stack */
    (GETBOX, spacer_index)         /* current_box[index] -> stack */
    (DOCPAGES)                     /* m_doc.pages -> stack */
    (ATE)                          /* (getbox(page) >= docpages()) -> stack */
    (CALL, command_call)           /* stack * numargs -> fun_name -> stack */
    (ADDCONTENT)                   /* stack -> content_stack */
    (POPCONTENT)                   /* content_stack.pop() */
    (SETBEGIN)                     /* stack -> content_stack.top.setbegin */
    (SETEND)                       /* stack -> content_stack.top.setend */
    (NEWVIEW)                      /* content_stack.top.newview() */
    (SPLITVIEW)                    /* content_stack.top.splitview() */
    (NEXTRESULT)                   /* content_stack.top.nextresult() */
    (RESETVIEW)                    /* content_stack.top.resetview() */
    (THROWERROR)                   /* stack -> throw */
    (WARNING)                      /* stack -> warnings */
    (IMPORT, import_options)       /* importa il file e lo esegue */
    (SETLANG, intl::language)      /* imposta la lingua del layout */
    (JMP, jump_address)            /* unconditional jump */
    (JZ, jump_address)             /* stack -> jump if top == 0 */
    (JNZ, jump_address)            /* stack -> jump if top != 0 */
    (JNTE, jump_address)           /* jump if content_stack.top at token end */
    (JSR, jsr_address)             /* program_counter -> call_stack -- jump to subroutine and discard return value */
    (JSRVAL, jsr_address)          /* program_counter -> call_stack -- jump to subroutine */
    (RET)                          /* jump to call_stack.top */
    (RETVAL)                       /* return to caller and push value to stack */
    (HLT)                          /* ferma l'esecuzione */
)

template<typename T, typename TList> struct type_in_list{};

template<typename T> struct type_in_list<T, TypeList<>> : std::false_type {};

template<typename T, typename First, typename ... Ts>
struct type_in_list<T, TypeList<First, Ts...>> :
    std::conditional_t<std::is_same_v<T, First>,
        std::true_type,
        type_in_list<T, TypeList<Ts...>>> {};

template<typename T, typename TList> struct add_if_unique{};
template<typename T, typename ... Ts> struct add_if_unique<T, TypeList<Ts...>> {
    using type = std::conditional_t<type_in_list<T, TypeList<Ts...>>::value || std::is_void_v<T>, TypeList<Ts...>, TypeList<Ts..., T>>;
};

template<typename TList> struct unique_types{};
template<typename T> using unique_types_t = typename unique_types<T>::type;
template<> struct unique_types<TypeList<>> {
    using type = TypeList<>;
};
template<typename First, typename ... Ts>
struct unique_types<TypeList<First, Ts...>> {
    using type = typename add_if_unique<First, unique_types_t<TypeList<Ts...>>>::type;
};

template<typename TList> struct type_list_variant{};
template<typename ... Ts> struct type_list_variant<TypeList<Ts...>> {
    using type = std::variant<std::monostate, Ts...>;
};

using opcode_variant = typename type_list_variant<unique_types_t<EnumTypeList<opcode>>>::type;

class command_args {
private:
    opcode m_command;
    opcode_variant m_data;

public:
    command_args(opcode command = opcode::NOP) : m_command(command) {}

    template<typename T>
    command_args(opcode command, T &&data) : m_command(command), m_data(std::forward<T>(data)) {}

    opcode command() const noexcept {
        return m_command;
    }
    
    template<opcode Cmd> const auto &get_args() const {
        return std::get<EnumType<Cmd>>(m_data);
    }

    template<typename Visitor> auto visit(Visitor func) const {
        return std::visit(func, m_data);
    }

    template<typename Visitor> auto visit(Visitor func) {
        return std::visit(func, m_data);
    }
};

template<opcode Cmd, typename ... Ts>
command_args make_command(Ts && ... args) {
    return command_args(Cmd, EnumType<Cmd>{ std::forward<Ts>(args) ... });
}

template<opcode Cmd> requires std::is_void_v<EnumType<Cmd>>
command_args make_command() {
    return command_args(Cmd);
}

using bytecode = std::vector<command_args>;

#endif