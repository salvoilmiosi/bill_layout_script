#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "intl.h"
#include "functions.h"

struct string_ptr : std::shared_ptr<std::string> {
    using base = std::shared_ptr<std::string>;
    string_ptr() = default;

    template<typename U>
    string_ptr(U && str) : base(std::make_shared<std::string>(std::forward<U>(str))) {}

    const std::string &string() const {
        static const std::string empty_string;
        if (base::operator bool()) {
            return *static_cast<base>(*this);
        } else {
            return empty_string;
        }
    }

    operator const std::string &() const {
        return string();
    }
};

struct command_call {
    function_iterator fun;
    small_int numargs;

    command_call(std::string_view name, int numargs) : fun(function_lookup.find(name)), numargs(numargs) {
        assert(fun != function_lookup.end());
    }
    command_call(function_iterator fun, int numargs) : fun(fun), numargs(numargs) {}
};

DEFINE_ENUM(spacer_index,
    (PAGE,      "p")
    (X,         "x")
    (Y,         "y")
    (WIDTH,     "width")
    (HEIGHT,    "height")
    (TOP,       "top")
    (RIGHT,     "right")
    (BOTTOM,    "bottom")
    (LEFT,      "left")
    (ROTATE,    "rotate")
)

DEFINE_ENUM_FLAGS(selvar_flags,
    (GLOBAL)
    (DYN_IDX)
    (DYN_LEN)
    (EACH)
    (APPEND)
)

struct variable_selector {
    string_ptr name;
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
    string_ptr label;
    int16_t relative_addr;

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
    string_ptr filename;
    bitset<import_flags> flags;
};

DEFINE_ENUM_TYPES(opcode,
    (NOP)                           // no operation
    (SETBOX, pdf_rect)              // imposta il box da leggere
    (MVBOX, spacer_index)           // stack -> current_box[index]
    (RDBOX)                         // poppler.get_text(current_box) -> content_stack
    (NEXTTABLE)                     // current_table++
    (SELVAR, variable_selector)     // (name, index, size, flags) -> selected
    (SETVAR, bitset<setvar_flags>)  // selected, stack -> set
    (CLEAR)                         // selected -> clear
    (GETSIZE)                       // selected -> size() -> stack
    (PUSHVAR)                       // selected -> stack
    (PUSHREF)                       // selected.str_view -> stack
    (PUSHVIEW)                      // content_stack -> stack
    (PUSHNUM, fixed_point)          // number -> stack
    (PUSHINT, int64_t)              // int -> stack
    (PUSHSTR, string_ptr)          // str -> stack
    (PUSHARG, small_int)            // stack -> stack
    (GETBOX, spacer_index)          // current_box[index] -> stack
    (DOCPAGES)                      // m_doc.pages -> stack
    (CALL, command_call)            // stack * numargs -> fun_name -> stack
    (ADDCONTENT)                    // stack -> content_stack
    (POPCONTENT)                    // content_stack.pop()
    (SETBEGIN)                      // stack -> content_stack.top.setbegin
    (SETEND)                        // stack -> content_stack.top.setend
    (NEWVIEW)                       // content_stack.top.newview()
    (SPLITVIEW)                     // content_stack.top.splitview()
    (NEXTRESULT)                    // content_stack.top.nextresult()
    (RESETVIEW)                     // content_stack.top.resetview()
    (THROWERROR)                    // stack -> throw
    (WARNING)                       // stack -> warnings
    (IMPORT, import_options)        // importa il file e lo esegue
    (SETLANG, intl::language)       // imposta la lingua del layout
    (JMP, jump_address)             // unconditional jump
    (JZ, jump_address)              // stack -> jump if top == 0
    (JNZ, jump_address)             // stack -> jump if top != 0
    (JNTE, jump_address)            // jump if content_stack.top at token end
    (JSR, jsr_address)              // program_counter -> call_stack -- jump to subroutine and discard return value
    (JSRVAL, jsr_address)           // program_counter -> call_stack -- jump to subroutine
    (RET)                           // jump to call_stack.top
    (RETVAL)                        // return to caller and push value to stack
    (HLT)                           // ferma l'esecuzione
)

template<typename T> using variant_type = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

template<string_enum Enum, typename ISeq> struct enum_variant_impl{};
template<string_enum Enum, size_t ... Is> struct enum_variant_impl<Enum, std::index_sequence<Is...>> {
    using type = std::variant<variant_type<EnumType<static_cast<Enum>(Is)>> ...>;
};

template<string_enum Enum> using enum_variant = typename enum_variant_impl<Enum, std::make_index_sequence<EnumSize<Enum>>>::type;

class command_args {
private:
    enum_variant<opcode> m_value;

public:
    command_args() = default;
    command_args(auto && arg) : m_value(std::forward<decltype(arg)>(arg)) {}

    opcode command() const noexcept {
        return static_cast<opcode>(m_value.index());
    }
    
    template<opcode Cmd> requires (! std::is_void_v<EnumType<Cmd>>)
    const auto &get_args() const {
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
    return enum_variant<opcode>(std::in_place_index<static_cast<size_t>(Cmd)>, std::forward<Ts>(data) ...);
}

using bytecode = std::vector<command_args>;

#endif