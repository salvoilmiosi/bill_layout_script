#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "binary_bls.h"

#include <boost/locale.hpp>

using namespace bls;

void reader::clear() {
    m_code.clear();
    m_flags = 0;
    m_doc = nullptr;
}

void reader::start() {
    m_values.clear();
    m_globals.clear();
    m_notes.clear();
    m_layouts.clear();
    m_stack.clear();
    m_contents.clear();
    m_selected.clear();

    m_values.emplace_back();
    m_current_table = m_values.begin();

    m_calls.clear();
    m_calls.emplace(std::numeric_limits<decltype(m_program_counter)>::max());

    m_current_box = {};

    m_locale = std::locale::classic();

    m_program_counter = 0;
    m_program_counter_next = 0;

    m_running = true;
    m_aborted = false;

    try {
        while (m_running && m_program_counter < m_code.size()) {
            m_program_counter_next = m_program_counter + 1;
            exec_command(m_code[m_program_counter]);
            m_program_counter = m_program_counter_next;
        }
    } catch (const layout_error &err) {
        throw reader_error(std::format("{0}: Ln {1}\n{2}\n{3}",
            m_box_name, m_last_line.line, m_last_line.comment,
            err.what()));
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

static void to_subitem(variable &var, small_int idx) {
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

size_t reader::add_layout(const std::filesystem::path &filename) {
    std::filesystem::path cache_filename = filename;
    cache_filename.replace_extension(".cache");

    auto is_modified = [&](const std::filesystem::path &file) {
        return std::filesystem::exists(file) && std::filesystem::last_write_time(file) > std::filesystem::last_write_time(cache_filename);
    };
    
    bytecode new_code;

    // Se settata flag USE_CACHE, leggi il file di cache e
    // ricompila solo se uno dei file importati è stato modificato.
    // Altrimenti ricompila sempre
    if (m_flags.check(reader_flags::USE_CACHE) && std::filesystem::exists(cache_filename) && !is_modified(filename)) {
        new_code = binary::read(cache_filename);
    } else {
        parser my_parser;
        my_parser.read_layout(filename, layout_box_list::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags.check(reader_flags::USE_CACHE)) {
            binary::write(new_code, cache_filename);
        }
    }

    return add_code(std::move(new_code));
}

size_t reader::add_code(bytecode &&new_code) {
    if (!m_code.empty()) {
        new_code.add_line<opcode::SETCURLAYOUT>(m_current_layout - m_layouts.begin());
        if (std::has_facet<boost::locale::info>(m_locale)) {
            new_code.add_line<opcode::SETLANG>(std::use_facet<boost::locale::info>(m_locale).name());
        } else {
            new_code.add_line<opcode::SETLANG>();
        }
    }
    new_code.add_line<opcode::RET>();

    return m_code.insert(m_code.end(), std::make_move_iterator(new_code.begin()), std::make_move_iterator(new_code.end())) - m_code.begin();
}

void reader::exec_command(const command_args &cmd) {
    visit_command(util::overloaded{
        [](command_tag<opcode::NOP>) {},
        [](command_tag<opcode::LABEL>, jump_label) {},
        [this](command_tag<opcode::BOXNAME>, const std::string &name) {
            m_box_name = name;
        },
        [this](command_tag<opcode::COMMENT>, const comment_line &line) {
            m_last_line = line;
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
        [this](command_tag<opcode::RDBOX>, readbox_options opts) {
            m_current_box.mode = opts.mode;
            m_current_box.flags = opts.flags;
            m_contents.emplace(get_document().get_text(m_current_box));
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
        [this](command_tag<opcode::SELINDEX>, small_int idx) {
            m_selected.top().add_index(idx);
        },
        [this](command_tag<opcode::SELINDEXDYN>) {
            m_selected.top().add_index(m_stack.pop()->as_int());
        },
        [this](command_tag<opcode::SELSIZE>, small_int size) {
            m_selected.top().set_size(size);
        },
        [this](command_tag<opcode::SELSIZEDYN>) {
            m_selected.top().set_size(m_stack.pop()->as_int());
        },
        [this](command_tag<opcode::SELAPPEND>) {
            m_selected.top().add_append();
        },
        [this](command_tag<opcode::SELEACH>) {
            m_selected.top().add_each();
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
        [this](command_tag<opcode::SUBITEM>, small_int idx) {
            to_subitem(m_stack.top(), idx);
        },
        [this](command_tag<opcode::SUBITEMDYN>) {
            small_int idx = m_stack.pop()->as_int();
            to_subitem(m_stack.top(), idx);
        },
        [this](command_tag<opcode::PUSHVAR>) {
            m_stack.push(m_selected.pop()->get_value());
        },
        [this](command_tag<opcode::PUSHVIEW>) {
            m_stack.push(m_contents.top().view());
        },
        [this](command_tag<opcode::PUSHNUM>, fixed_point num) {
            m_stack.push(num);
        },
        [this](command_tag<opcode::PUSHINT>, big_int num) {
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
        [this](command_tag<opcode::CALL>, const command_call &call) {
            m_stack.push(call.fun->second(this, m_stack, call.numargs));
        },
        [this](command_tag<opcode::SYSCALL>, const command_call &call) {
            call.fun->second(this, m_stack, call.numargs);
        },
        [this](command_tag<opcode::CNTADD>) {
            m_contents.emplace(std::move(*m_stack.pop()));
        },
        [this](command_tag<opcode::CNTADDLIST>) {
            m_contents.emplace(std::move(*m_stack.pop()), as_array_tag);
        },
        [this](command_tag<opcode::CNTPOP>) {
            m_contents.pop();
        },
        [this](command_tag<opcode::NEXTRESULT>) {
            m_contents.top().nextresult();
        },
        [this](command_tag<opcode::JMP>, const jump_address &address) {
            jump_to(address);
        },
        [this](command_tag<opcode::JZ>, const jump_address &address) {
            if (!m_stack.pop()->is_true()) {
                jump_to(address);
            }
        },
        [this](command_tag<opcode::JNZ>, const jump_address &address) {
            if (m_stack.pop()->is_true()) {
                jump_to(address);
            }
        },
        [this](command_tag<opcode::JTE>, const jump_address &address) {
            if (m_contents.top().tokenend()) {
                jump_to(address);
            }
        },
        [this](command_tag<opcode::JSR>, const jump_address &address) {
            jump_subroutine(address);
        },
        [this](command_tag<opcode::JSRVAL>, const jump_address &address) {
            jump_subroutine(address, true);
        },
        [this](command_tag<opcode::RET>) {
            auto fun_call = m_calls.pop();
            m_program_counter_next = fun_call->return_addr;

            if (fun_call->getretvalue) {
                m_stack.emplace();
            }
        },
        [this](command_tag<opcode::RETVAL>) {
            auto fun_call = m_calls.pop();
            m_program_counter_next = fun_call->return_addr;

            if (!fun_call->getretvalue) {
                m_stack.pop();
            }
        },
        [this](command_tag<opcode::IMPORT>, const std::string &path) {
            jump_address addr = add_layout(path) - m_program_counter;
            m_code[m_program_counter] = make_command<opcode::JSR>(addr);
            jump_subroutine(addr);
        },
        [this](command_tag<opcode::ADDLAYOUT>, const std::string &path) {
            m_layouts.push_back(path);
            m_code[m_program_counter] = make_command<opcode::SETCURLAYOUT>(m_layouts.size() - 1);
            m_current_layout = m_layouts.end() - 1;
        },
        [this](command_tag<opcode::SETCURLAYOUT>, small_int idx) {
            m_current_layout = m_layouts.begin() + idx;
        },
        [this](command_tag<opcode::SETLANG>, const std::string &lang) {
            try {
                m_locale = boost::locale::generator{}(lang);
            } catch (std::runtime_error) {
                throw layout_error(intl::format("UNSUPPORTED_LANGUAGE", lang));
            }
        },
        [this](command_tag<opcode::FOUNDLAYOUT>) {
            if (m_flags.check(reader_flags::FIND_LAYOUT)) {
                m_running = false;
            }
        },
    }, cmd);
}