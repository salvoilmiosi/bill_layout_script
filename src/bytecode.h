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

template<typename T, typename TList> struct add_to_list{};
template<typename T, typename TList> using add_to_list_t = typename add_to_list<T, TList>::type;
template<typename T, typename ... Ts> struct add_to_list<T, TypeList<Ts...>> {
    using type = TypeList<T, Ts...>;
};

template<typename TList> struct variant_types{};
template<typename TList> using variant_types_t = typename variant_types<TList>::type;
template<> struct variant_types<TypeList<>> {
    using type = TypeList<>;
};
template<typename First, typename ... Ts> struct variant_types<TypeList<First, Ts...>> {
    using type = add_to_list_t<
        std::conditional_t<std::is_void_v<First>,
            std::monostate, First>,
        variant_types_t<TypeList<Ts...>>>;
};

template<typename TList> struct type_list_variant{};
template<typename ... Ts> struct type_list_variant<TypeList<Ts...>> {
    using type = std::variant<Ts...>;
};

using opcode_variant = typename type_list_variant<variant_types_t<EnumTypeList<opcode>>>::type;

class command_args {
private:
    opcode_variant m_value;

public:
    command_args() = default;
    command_args(auto && arg) : m_value(std::forward<decltype(arg)>(arg)) {}

    opcode command() const noexcept {
        return static_cast<opcode>(m_value.index());
    }
    
    template<opcode Cmd> const auto &get_args() const {
        return std::get<static_cast<size_t>(Cmd)>(m_value);
    }

    template<typename Visitor> auto visit(Visitor func) const {
        return std::visit(func, m_value);
    }

    template<typename Visitor> auto visit(Visitor func) {
        return std::visit(func, m_value);
    }
};

template<opcode Cmd, typename ... Ts>
command_args make_command(Ts && ... data) {
    return opcode_variant(std::in_place_index<static_cast<size_t>(Cmd)>, std::forward<Ts>(data) ...);
}

using bytecode = std::vector<command_args>;

#endif