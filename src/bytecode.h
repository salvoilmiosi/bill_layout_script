#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "functions.h"
#include "exceptions.h"

namespace bls {
    typedef uint8_t small_int;

    struct command_call {
        function_iterator fun;
        small_int numargs;

        command_call(std::string_view name, int numargs) : fun(function_lookup::find(name)), numargs(numargs) {
            assert(function_lookup::valid(fun));
        }
        command_call(function_iterator fun, int numargs) : fun(fun), numargs(numargs) {}
    };

    DEFINE_ENUM_DATA_IN_NS(bls, spacer_index, static_vector<const char *>,
        (PAGE,      "p", "page")
        (X,         "x")
        (Y,         "y")
        (WIDTH,     "w", "width")
        (HEIGHT,    "h", "height")
        (TOP,       "t", "top")
        (RIGHT,     "r", "right")
        (BOTTOM,    "b", "bottom")
        (LEFT,      "l", "left")
        (ROTATE,    "rot", "rotate")
    )

    struct readbox_options {
        read_mode mode;
        enums::bitset<box_flags> flags;
    };

    using jump_label = util::strong_typedef<std::string>;

    struct jump_address {
        jump_label label;
        int16_t address = 0;

        jump_address() = default;
        jump_address(const jump_label &label) : label(label) {}
        jump_address(jump_label &&label) : label(std::move(label)) {}
        jump_address(int16_t address) : address(address) {}
    };

    DEFINE_ENUM_TYPES_IN_NS(bls, opcode,
        (NOP)                           // no operation
        (COMMENT, std::string)          // comment
        (LABEL, jump_label)             // jump label
        (NEWBOX)                        // resetta current_box
        (MVBOX, spacer_index)           // stack -> current_box[index]
        (MVNBOX, spacer_index)          // -stack -> current_box[index]
        (RDBOX, readbox_options)        // poppler.get_text(current_box) -> content_stack
        (SELVAR, std::string)
        (SELVARDYN)
        (SELGLOBAL, std::string)
        (SELGLOBALDYN)
        (SELLOCAL, std::string)
        (SELLOCALDYN)
        (SELINDEX, small_int)
        (SELINDEXDYN)
        (SELAPPEND)
        (SELEACH)
        (SELSIZE, small_int)
        (SELSIZEDYN)
        (SETVAR)                        // stack -> selected = stack
        (FORCEVAR)                      // stack -> selected = stack (force)
        (INCVAR)                        // stack -> selected += stack
        (DECVAR)                        // stack -> selected -= stack
        (CLEAR)                         // selected -> clear
        (SUBITEM, small_int)
        (SUBITEMDYN)
        (PUSHVAR)                       // selected -> stack
        (PUSHVIEW)                      // content_stack -> stack
        (PUSHNUM, fixed_point)          // number -> stack
        (PUSHINT, big_int)              // int -> stack
        (PUSHDOUBLE, double)            // double -> stack
        (PUSHSTR, std::string)          // str -> stack
        (PUSHREGEX, std::string)        // str -> stack (flag come regex)
        (CALL, command_call)            // stack * numargs -> fun_name -> stack
        (SYSCALL, command_call)         // stack * numargs -> fun_name
        (CNTADD)                        // stack -> content_stack
        (CNTADDLIST)                    // stack -> content_stack
        (CNTPOP)                        // content_stack.pop()
        (NEXTRESULT)                    // content_stack.top.nextresult()
        (JMP, jump_address)             // unconditional jump
        (JZ, jump_address)              // stack -> jump if top == 0
        (JNZ, jump_address)             // stack -> jump if top != 0
        (JTE, jump_address)             // jump if content_stack.top at token end
        (JSR, jump_address)             // program_counter -> call_stack -- jump to subroutine and discard return value
        (JSRVAL, jump_address)          // program_counter -> call_stack -- jump to subroutine
        (RET)                           // jump to call_stack.top
        (RETVAL)                        // return to caller and push value to stack
        (IMPORT, std::string)           // importa il file e lo esegue
        (ADDLAYOUT, std::string)        // aggiunge il nome del layout nella lista di output
        (SETCURLAYOUT, int)             // sposta il puntatore del layout corrente
        (SETLAYOUT)                     // ferma l'esecuzione se settata la flag setlayout in reader
        (SETLANG, std::string)          // imposta il locale corrente
    )

    template<typename T> using variant_type = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

    namespace detail {
        template<enums::type_enum Enum, typename ISeq> struct enum_variant{};
        template<enums::type_enum Enum, size_t ... Is> struct enum_variant<Enum, std::index_sequence<Is...>> {
            using type = std::variant<variant_type<enums::get_type_t<static_cast<Enum>(Is)>> ...>;
        };
    }

    template<enums::type_enum Enum> using enum_variant = typename detail::enum_variant<
        Enum, std::make_index_sequence<enums::size<Enum>()>>::type;

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
        
        template<opcode Cmd> requires (! std::is_void_v<enums::get_type_t<Cmd>>)
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

        bytecode::const_iterator find_label(const jump_label &label) {
            return std::ranges::find_if(*this, [&](const command_args &line) {
                return line.command() == opcode::LABEL && line.get_args<opcode::LABEL>() == label;
            });
        }

        void add_label(const jump_label &label) {
            if (find_label(label) == end()) {
                add_line<opcode::LABEL>(label);
            } else {
                throw layout_error(intl::format("DUPLICATE_GOTO_LABEL", label));
            }
        }

        void move_not_comments(size_t pos_begin, size_t pos_end) {
            auto it_begin = begin() + pos_begin;
            auto it_end = begin() + pos_end;
            std::rotate(std::find_if_not(it_begin, it_end, [](const command_args &cmd) {
                return cmd.command() == opcode::COMMENT;
            }), it_end, end());
        }

        command_args &last_not_comment() {
            return *std::find_if_not(rbegin(), rend(), [](const command_args &cmd) {
                return cmd.command() == opcode::COMMENT;
            });
        }
    };

}

#endif