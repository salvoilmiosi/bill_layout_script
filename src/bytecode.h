#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "functions.h"
#include "string_ptr.h"

typedef uint8_t small_int;

struct command_call {
    function_iterator fun;
    small_int numargs;

    command_call(std::string_view name, int numargs) : fun(function_lookup.find(name)), numargs(numargs) {
        assert(fun != function_lookup.end());
    }
    command_call(function_iterator fun, int numargs) : fun(fun), numargs(numargs) {}
};

DEFINE_ENUM(spacer_index,
    (PAGE,      static_vector{"p", "page"})
    (X,         static_vector{"x"})
    (Y,         static_vector{"y"})
    (WIDTH,     static_vector{"w", "width"})
    (HEIGHT,    static_vector{"h", "height"})
    (TOP,       static_vector{"t", "top"})
    (RIGHT,     static_vector{"r", "right"})
    (BOTTOM,    static_vector{"b", "bottom"})
    (LEFT,      static_vector{"l", "left"})
    (ROTATE,    static_vector{"rot", "rotate"})
)

struct readbox_options {
    read_mode mode;
    box_type type;
    bitset<box_flags> flags;
};

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
    (DECREASE)
)

typedef std::variant<ptrdiff_t, string_ptr> jump_address;

struct jsr_address : jump_address {
    small_int numargs;
};

DEFINE_ENUM_TYPES(opcode,
    (NOP)                           // no operation
    (COMMENT, string_ptr)           // comment
    (LABEL, string_ptr)             // jump label
    (NEWBOX)                        // resetta current_box
    (MVBOX, spacer_index)           // stack -> current_box[index]
    (RDBOX, readbox_options)        // poppler.get_text(current_box) -> content_stack
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
    (PUSHDOUBLE, double)            // double -> stack
    (PUSHSTR, string_ptr)           // str -> stack
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
    (JMP, jump_address)             // unconditional jump
    (JZ, jump_address)              // stack -> jump if top == 0
    (JNZ, jump_address)             // stack -> jump if top != 0
    (JNTE, jump_address)            // jump if content_stack.top at token end
    (JSR, jsr_address)              // program_counter -> call_stack -- jump to subroutine and discard return value
    (JSRVAL, jsr_address)           // program_counter -> call_stack -- jump to subroutine
    (RET)                           // jump to call_stack.top
    (RETVAL)                        // return to caller and push value to stack
    (IMPORT, string_ptr)            // importa il file e lo esegue
    (LAYOUTNAME, string_ptr)        // aggiunge il nome del layout nella lista di output
    (SETLAYOUT)                     // ferma l'esecuzione se settata la flag setlayout in reader
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

    template<size_t I, typename ... Ts>
    command_args(std::in_place_index_t<I> idx, Ts && ... args) : m_value(idx, std::forward<Ts>(args) ...) {}

public:
    command_args() = default;
    template<opcode Cmd, typename ... Ts>
    friend command_args make_command(Ts && ... args);

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
command_args make_command(Ts && ... args) {
    return command_args(std::in_place_index<static_cast<size_t>(Cmd)>, std::forward<Ts>(args) ...);
}

struct bytecode : std::vector<command_args> {
    bytecode() {
        reserve(4096);
    }
    
    template<opcode Cmd, typename ... Ts>
    void add_line(Ts && ... args) {
        push_back(make_command<Cmd>(std::forward<Ts>(args) ... ));
    }

    bytecode::const_iterator find_label(string_ptr label) {
        return std::ranges::find_if(*this, [&](const command_args &line) {
            return line.command() == opcode::LABEL && line.get_args<opcode::LABEL>() == label;
        });
    }

    void add_label(string_ptr label) {
        if (find_label(label) == end()) {
            add_line<opcode::LABEL>(label);
        } else {
            throw std::runtime_error(fmt::format("Etichetta goto duplicata: {}", *label));
        }
    }
};

#endif