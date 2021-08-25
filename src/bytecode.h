#ifndef __BYTECODE_H__
#define __BYTECODE_H__

#include <list>

#include "pdf_document.h"
#include "utils.h"
#include "fixed_point.h"
#include "functions.h"
#include "exceptions.h"

namespace bls {
    struct command_call : function_iterator {
        command_call(function_iterator fun) : function_iterator(fun) {}
        command_call(std::string_view name) : function_iterator(function_lookup::find(name)) {
            assert(function_lookup::valid(*this));
        }
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

    struct command_label {
        int id;
    };

    using command_list_base = std::list<class command_args>;
    using command_node = command_list_base::iterator;
    
    using string_container = std::list<std::string>;
    using string_ptr = string_container::const_iterator;

    DEFINE_ENUM_TYPES_IN_NS(bls, opcode,
        (NOP)                           // no operation
        (LABEL, command_label)          // command label
        (BOXNAME, string_ptr)           // set box name
        (COMMENT, string_ptr)           // comment
        (NEWBOX)                        // resetta current_box
        (MVBOX, spacer_index)           // stack -> current_box[index]
        (MVNBOX, spacer_index)          // -stack -> current_box[index]
        (RDBOX, readbox_options)        // poppler.get_text(current_box) -> view_stack
        (SELVAR, string_ptr)            // name -> selected (current_table)
        (SELVARDYN)                     // stack -> selected (current_table)
        (SELGLOBAL, string_ptr)         // name -> selected (globals)
        (SELGLOBALDYN)                  // stack -> selected (globals)
        (SELLOCAL, string_ptr)          // name -> selected (calls.top.vars)
        (SELLOCALDYN)                   // stack -> selected (calls.top.vars)
        (SELINDEX, size_t)              // index -> selected.add_index
        (SELINDEXDYN)                   // stack -> selected.add_index
        (SELAPPEND)                     // selected.add_append
        (FWDVAR)                        // stack -> selected = stack (forward)
        (SETVAR)                        // stack -> selected = stack (copy if not null)
        (FORCEVAR)                      // stack -> selected = stack (force)
        (INCVAR)                        // stack -> selected += stack (if not null)
        (DECVAR)                        // stack -> selected -= stack (if not null)
        (CLEAR)                         // selected -> clear
        (SUBITEM, size_t)               // stack.top = stack.top[index]
        (SUBITEMDYN)                    // stack -> stack.top = stack.top[stack]
        (PUSHVAR)                       // selected -> stack
        (PUSHVIEW)                      // view_stack -> stack
        (PUSHNUM, fixed_point)          // number -> stack
        (PUSHBOOL, bool)                // bool -> stack
        (PUSHINT, int64_t)              // int -> stack
        (PUSHDOUBLE, double)            // double -> stack
        (PUSHSTR, string_ptr)           // str -> stack
        (PUSHREGEX, string_ptr)         // str -> stack (flag come regex)
        (CALLARGS, size_t)              // sets numargs
        (CALL, command_call)            // stack * numargs -> fun_name -> stack
        (SYSCALL, command_call)         // stack * numargs -> fun_name
        (VIEWADD)                       // stack -> view_stack
        (VIEWADDLIST)                   // stack -> view_stack
        (VIEWPOP)                       // view_stack.pop()
        (VIEWNEXT)                      // view_stack.top.nextview()
        (JMP, command_node)             // unconditional jump
        (JZ, command_node)              // stack -> jump if top == 0
        (JNZ, command_node)             // stack -> jump if top != 0
        (JVE, command_node)             // jump if view_stack.top at end
        (JSR, command_node)             // program_counter -> call_stack -- jump to subroutine and discard return value
        (JSRVAL, command_node)          // program_counter -> call_stack -- jump to subroutine
        (MOVERVAL)                      // moves return value
        (COPYRVAL)                      // copies return value
        (RET)                           // jump to call_stack.top
        (IMPORT, string_ptr)            // importa il file e lo esegue
        (SETPATH, string_ptr)           // aggiunge il percorso del layout nella lista di output
        (SETLANG, string_ptr)           // imposta il locale corrente
        (FOUNDLAYOUT)                   // ferma l'esecuzione se settata la flag find layout in reader
    )

    class command_args {
    private:
        util::enum_variant<opcode> m_value;

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

        template<opcode Cmd> requires (! std::is_void_v<enums::get_type_t<Cmd>>)
        auto &get_args() {
            return std::get<static_cast<size_t>(Cmd)>(m_value);
        }
    };

    template<opcode Cmd> struct command_tag {};

    template<typename ReturnType, typename Command, typename Function>
    requires std::same_as<std::remove_const_t<Command>, command_args>
    ReturnType visit_command(Function &&fun, Command &cmd) {
        static constexpr auto command_vtable = []<size_t ... Is>(std::index_sequence<Is...>) {
            return std::array{ +[](Function &fun, Command &cmd) -> ReturnType {
                constexpr opcode Cmd = static_cast<opcode>(Is);
                if constexpr (std::is_void_v<enums::get_type_t<Cmd>>) {
                    return std::invoke(fun, command_tag<Cmd>{});
                } else if constexpr (std::is_same_v<enums::get_type_t<Cmd>, string_ptr>) {
                    return std::invoke(fun, command_tag<Cmd>{}, *cmd.template get_args<Cmd>());
                } else {
                    return std::invoke(fun, command_tag<Cmd>{}, cmd.template get_args<Cmd>());
                }
            } ... };
        } (std::make_index_sequence<enums::size<opcode>()>{});

        return command_vtable[static_cast<size_t>(cmd.command())](fun, cmd);
    }
    
    template<opcode Cmd, typename Function> struct command_return_type {
        using type = std::invoke_result_t<Function, command_tag<Cmd>, std::add_lvalue_reference_t<enums::get_type_t<Cmd>>>;
    };
    template<opcode Cmd, typename Function> requires std::is_same_v<enums::get_type_t<Cmd>, string_ptr>
    struct command_return_type<Cmd, Function> {
        using type = std::invoke_result_t<Function, command_tag<Cmd>, const std::string &>;
    };
    template<opcode Cmd, typename Function> requires std::is_void_v<enums::get_type_t<Cmd>>
    struct command_return_type<Cmd, Function> {
        using type = std::invoke_result_t<Function, command_tag<Cmd>>;
    };
    template<opcode Cmd, typename Function> using command_return_type_t = typename command_return_type<Cmd, Function>::type;

    template<typename Command, typename Function>
    requires std::same_as<std::remove_const_t<Command>, command_args>
    decltype(auto) visit_command(Function &&fun, Command &cmd) {
        using return_type = command_return_type_t<static_cast<opcode>(0), Function>;
        return visit_command<return_type>(std::move(fun), cmd);
    }

    template<opcode Cmd, typename ... Ts>
    command_args make_command(Ts && ... args) {
        return command_args(std::in_place_index<static_cast<size_t>(Cmd)>, std::forward<Ts>(args) ...);
    }

    struct command_list : command_list_base {
        string_container string_data;

        template<opcode Cmd, typename ... Ts>
        command_args new_line(Ts && ... args) {
            return make_command<Cmd>(std::forward<Ts>(args) ... );
        }

        template<opcode Cmd, typename ... Ts> requires std::is_same_v<enums::get_type_t<Cmd>, string_ptr>
        command_args new_line(Ts && ... args) {
            return make_command<Cmd>(string_data.emplace(string_data.end(), std::forward<Ts>(args) ... ));
        }

        template<opcode Cmd, typename ... Ts>
        void add_line(Ts && ... args) {
            push_back(new_line<Cmd>(std::forward<Ts>(args) ... ));
        }
    };

}

#endif