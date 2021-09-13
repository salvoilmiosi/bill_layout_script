#include "reader.h"

#include "parser.h"
#include "bytecode_printer.h"

#include <boost/locale.hpp>

using namespace bls;

void reader::clear() {
    m_code.clear();
    m_flags.clear();
    m_doc = nullptr;
}

void reader::start() {
    m_values.clear();
    m_globals.clear();
    m_notes.clear();
    m_layouts.clear();
    m_stack.clear();
    m_views.clear();
    m_selected.clear();

    m_values.emplace_back();
    m_current_table = m_values.begin();

    m_calls.clear();
    m_calls.emplace();

    m_box_name = nullptr;
    m_last_line = nullptr;

    m_current_box = {};

    m_locale = std::locale::classic();

    m_program_counter = m_program_counter_next = m_code.begin();

    m_running = true;
    m_aborted = false;

    try {
        while (m_running) {
            m_program_counter_next = std::next(m_program_counter);
            exec_command(*m_program_counter);
            m_program_counter = m_program_counter_next;
        }
    } catch (const layout_error &err) {
        if (m_box_name && m_last_line) {
            throw reader_error(std::format("{}: {}\n{}", *m_box_name, *m_last_line, err.what()));
        } else {
            throw reader_error(err.what());
        }
    }
    if (m_aborted) {
        throw reader_aborted{};
    }
    
    m_running = false;
}

static void move_box(pdf_rect &box, spacer_index idx, const variable &amt) {
    switch (idx) {
    case spacer_index::PAGE:
        box.page += amt.as_int(); break;
    case spacer_index::ROTATE:
        box.rotate(amt.as_int()); break;
    case spacer_index::X:
        box.x += amt.as_double(); break;
    case spacer_index::Y:
        box.y += amt.as_double(); break;
    case spacer_index::WIDTH:
    case spacer_index::RIGHT:
        box.w += amt.as_double(); break;
    case spacer_index::HEIGHT:
    case spacer_index::BOTTOM:
        box.h += amt.as_double(); break;
    case spacer_index::TOP:
        box.y += amt.as_double();
        box.h -= amt.as_double();
        break;
    case spacer_index::LEFT:
        box.x += amt.as_double();
        box.w -= amt.as_double();
        break;
    }
}

static void to_subitem(variable &var, size_t idx) {
    if (var.is_array()) {
        const auto &arr = var.deref().as_array();
        if (arr.size() > idx) {
            if (var.is_pointer()) {
                var = arr[idx].as_pointer();
            } else {
                var = arr[idx];
            }
            return;
        }
    }
    var = variable();
}

constexpr auto is_label = [](const command_args &cmd) {
    return cmd.command() == opcode::LABEL;
};

command_node reader::add_layout(const layout_box_list &layout) {
    auto new_code = parser{}(layout);

    for (command_args &line : new_code) {
        visit_command(util::overloaded{
            []<opcode Cmd>(command_tag<Cmd>) {},
            []<opcode Cmd>(command_tag<Cmd>, auto &) {},
            []<opcode Cmd>(command_tag<Cmd>, command_node &node) {
                for (; is_label(*node); ++node);
            }
        }, line);
    }
    new_code.remove_if(is_label);

    auto loc = new_code.begin();
    m_code.string_data.splice(m_code.string_data.end(), std::move(new_code.string_data));
    m_code.splice(m_code.end(), std::move(new_code));
    return loc;
}

variable reader::do_function_call(const command_call &call) {
    if (call->second.minargs == call->second.maxargs) {
        m_numargs = call->second.minargs;
    }
    auto ret = call->second(this, arg_list(m_stack.end() - m_numargs, m_stack.end()));
    m_stack.resize(m_stack.size() - m_numargs);
    return ret;
}

void reader::exec_command(const command_args &cmd) {
    visit_command(util::overloaded{
        [](command_tag<opcode::NOP>) {},
        [](command_tag<opcode::LABEL>, auto) {},
        [this](command_tag<opcode::BOXNAME>, const std::string &name) {
            m_box_name = &name;
        },
        [this](command_tag<opcode::COMMENT>, const std::string &line) {
            m_last_line = &line;
        },
        [this](command_tag<opcode::NEWBOX>) {
            m_current_box = {};
        },
        [this](command_tag<opcode::MVBOX>, spacer_index idx) {
            move_box(m_current_box, idx, *m_stack.pop());
        },
        [this](command_tag<opcode::MVNBOX>, spacer_index idx) {
            move_box(m_current_box, idx, -(*m_stack.pop()));
        },
        [this](command_tag<opcode::RDBOX>, read_mode mode) {
            m_current_box.mode = mode;
            m_stack.emplace(get_document().get_text(m_current_box));
        },
        [this](command_tag<opcode::RDPAGE>, read_mode mode) {
            m_current_box.mode = mode;
            m_stack.emplace(get_document().get_page_text(m_current_box));
        },
        [this](command_tag<opcode::SELVAR>, const std::string &name) {
            m_selected.emplace(*m_current_table, name);
        },
        [this](command_tag<opcode::SELVARDYN>) {
            m_selected.emplace(*m_current_table, std::move(*m_stack.pop()).as_string());
        },
        [this](command_tag<opcode::SELGLOBAL>, const std::string &name) {
            m_selected.emplace(m_globals, name);
        },
        [this](command_tag<opcode::SELGLOBALDYN>) {
            m_selected.emplace(m_globals, std::move(*m_stack.pop()).as_string());
        },
        [this](command_tag<opcode::SELLOCAL>, const std::string &name) {
            m_selected.emplace(m_calls.top().vars, name);
        },
        [this](command_tag<opcode::SELLOCALDYN>) {
            m_selected.emplace(m_calls.top().vars, std::move(*m_stack.pop()).as_string());
        },
        [this](command_tag<opcode::SELINDEX>, size_t idx) {
            m_selected.top().add_index(idx);
        },
        [this](command_tag<opcode::SELINDEXDYN>) {
            m_selected.top().add_index(m_stack.pop()->as_int());
        },
        [this](command_tag<opcode::SELAPPEND>) {
            m_selected.top().add_append();
        },
        [this](command_tag<opcode::FWDVAR>) {
            m_selected.pop()->fwd_value(std::move(*m_stack.pop()));
        },
        [this](command_tag<opcode::SETVAR>) {
            m_selected.pop()->set_value(std::move(*m_stack.pop()));
        },
        [this](command_tag<opcode::FORCEVAR>) {
            m_selected.pop()->force_value(std::move(*m_stack.pop()));
        },
        [this](command_tag<opcode::INCVAR>) {
            m_selected.pop()->inc_value(*m_stack.pop());
        },
        [this](command_tag<opcode::DECVAR>) {
            m_selected.pop()->dec_value(*m_stack.pop());
        },
        [this](command_tag<opcode::CLEAR>) {
            m_selected.pop()->clear_value();
        },
        [this](command_tag<opcode::SUBITEM>, size_t idx) {
            to_subitem(m_stack.top(), idx);
        },
        [this](command_tag<opcode::SUBITEMDYN>) {
            size_t idx = m_stack.pop()->as_int();
            to_subitem(m_stack.top(), idx);
        },
        [this](command_tag<opcode::PUSHVAR>) {
            m_stack.push(m_selected.pop()->get_value());
        },
        [this](command_tag<opcode::PUSHVIEW>) {
            m_stack.push(m_views.top().view());
        },
        [this](command_tag<opcode::PUSHNULL>) {
            m_stack.emplace();
        },
        [this](command_tag<opcode::PUSHNUM>, fixed_point num) {
            m_stack.push(num);
        },
        [this](command_tag<opcode::PUSHBOOL>, bool value) {
            m_stack.push(value);
        },
        [this](command_tag<opcode::PUSHINT>, int64_t num) {
            m_stack.push(num);
        },
        [this](command_tag<opcode::PUSHDOUBLE>, double num) {
            m_stack.push(num);
        },
        [this](command_tag<opcode::PUSHSTR>, const std::string &str) {
            m_stack.push(std::string_view(str));
        },
        [this](command_tag<opcode::PUSHREGEX>, const std::string &str) {
            m_stack.emplace(std::string_view(str), as_regex_tag);
        },
        [this](command_tag<opcode::STKAPP>) {
            auto top = m_stack.pop()->deref();
            auto &var = m_stack.top();
            if (var.is_array()) {
                var.as_array().push_back(std::move(top));
            } else {
                var = variable_array{std::move(top)};
            }
        },
        [this](command_tag<opcode::STKSWP>) {
            std::swap(m_stack.top(), *(m_stack.end() - 2));
        },
        [this](command_tag<opcode::CALLARGS>, size_t numargs) {
            m_numargs = numargs;
        },
        [this](command_tag<opcode::CALL>, const command_call &call) {
            m_stack.push(do_function_call(call));
        },
        [this](command_tag<opcode::SYSCALL>, const command_call &call) {
            do_function_call(call);
        },
        [this](command_tag<opcode::VIEWADD>) {
            m_views.emplace(m_stack.top());
        },
        [this](command_tag<opcode::VIEWADDLIST>) {
            m_views.emplace(m_stack.top(), as_array_tag);
        },
        [this](command_tag<opcode::VIEWPOP>) {
            m_views.pop();
            m_stack.pop();
        },
        [this](command_tag<opcode::VIEWNEXT>) {
            m_views.top().nextview();
        },
        [this](command_tag<opcode::JMP>, command_node node) {
            jump_to(node);
        },
        [this](command_tag<opcode::JZ>, command_node node) {
            if (!m_stack.pop()->is_true()) {
                jump_to(node);
            }
        },
        [this](command_tag<opcode::JNZ>, command_node node) {
            if (m_stack.pop()->is_true()) {
                jump_to(node);
            }
        },
        [this](command_tag<opcode::JVE>, command_node node) {
            if (m_views.top().ate()) {
                jump_to(node);
            }
        },
        [this](command_tag<opcode::JSR>, command_node node) {
            jump_subroutine(node);
        },
        [this](command_tag<opcode::JSRVAL>, command_node node) {
            jump_subroutine(node, true);
        },
        [this](command_tag<opcode::COPYRVAL>) {
            m_calls.top().return_value = m_stack.pop()->deref();
        },
        [this](command_tag<opcode::MOVERVAL>) {
            m_calls.top().return_value = std::move(*m_stack.pop());
        },
        [this](command_tag<opcode::RET>) {
            if (m_calls.size() > 1) {
                auto fun_call = m_calls.pop();
                jump_to(fun_call->return_addr);
                if (fun_call->getretvalue) {
                    m_stack.push_back(std::move(fun_call->return_value));
                }
            } else {
                m_running = false;
            }
        },
        [this](command_tag<opcode::IMPORT>, const std::string &path) {
            auto node = add_layout(layout_box_list(path));
            *m_program_counter = make_command<opcode::JSR>(node);
            jump_subroutine(node);
        },
        [this](command_tag<opcode::SETPATH>, const std::string &path) {
            m_current_layout = m_layouts.emplace(path).first;
        },
        [this](command_tag<opcode::SETLANG>, const std::string &lang) {
            try {
                m_locale = boost::locale::generator{}(lang);
            } catch (std::runtime_error) {
                throw layout_error(intl::translate("UNSUPPORTED_LANGUAGE", lang));
            }
        },
        [this](command_tag<opcode::FOUNDLAYOUT>) {
            if (m_flags.check(reader_flags::FIND_LAYOUT)) {
                m_running = false;
            }
        },
    }, cmd);
}