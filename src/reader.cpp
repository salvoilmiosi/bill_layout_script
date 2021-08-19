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

void reader::exec_command(const command_args &cmd) {
    auto jump_to = [&](const jump_address &label) {
        m_program_counter_next = m_program_counter + label.address;
    };

    auto jump_subroutine = [&](const jump_address &address, bool getretvalue = false) {
        m_calls.emplace(m_program_counter + 1, getretvalue);
        jump_to(address);
    };

    auto jump_return = [&] {
        const auto &fun_call = m_calls.pop();
        m_program_counter_next = fun_call.return_addr;

        if (fun_call.getretvalue) {
            m_stack.emplace();
        }
    };

    auto jump_return_value = [&] {
        const auto &fun_call = m_calls.top();
        m_program_counter_next = fun_call.return_addr;

        if (fun_call.getretvalue) {
            m_stack.top() = m_stack.top().deref();
        } else {
            m_stack.pop();
        }
        m_calls.pop();
    };

    auto move_box = [&](spacer_index idx, variable &&amt) {
        switch (idx) {
        case spacer_index::PAGE:
            m_current_box.page += amt.as_int(); break;
        case spacer_index::ROTATE:
            m_current_box.rotate(amt.as_int()); break;
        case spacer_index::X:
            m_current_box.x += amt.as_double(); break;
        case spacer_index::Y:
            m_current_box.y += amt.as_double(); break;
        case spacer_index::WIDTH:
        case spacer_index::RIGHT:
            m_current_box.w += amt.as_double(); break;
        case spacer_index::HEIGHT:
        case spacer_index::BOTTOM:
            m_current_box.h += amt.as_double(); break;
        case spacer_index::TOP:
            m_current_box.y += amt.as_double();
            m_current_box.h -= amt.as_double();
            break;
        case spacer_index::LEFT:
            m_current_box.x += amt.as_double();
            m_current_box.w -= amt.as_double();
            break;
        }
    };

    auto read_box = [&](readbox_options opts) {
        m_current_box.mode = opts.mode;
        m_current_box.flags = opts.flags;

        m_contents.emplace(get_document().get_text(m_current_box));
    };

    auto call_function = [&](const command_call &cmd) {
        return cmd.fun->second(this, m_stack, cmd.numargs);
    };

    auto import_layout = [&](const std::filesystem::path &filename) {
        jump_address addr = add_layout(filename) - m_program_counter;
        m_code[m_program_counter] = make_command<opcode::JSR>(addr);
        jump_subroutine(addr);
    };

    auto push_layout = [&](const std::filesystem::path &filename) {
        m_layouts.push_back(filename);
        m_code[m_program_counter] = make_command<opcode::SETCURLAYOUT>(m_layouts.size() - 1);
        m_current_layout = m_layouts.end() - 1;
    };

    auto set_language = [&](const std::string &name) {
        try {
            m_locale = boost::locale::generator{}(name);
        } catch (std::runtime_error) {
            throw layout_error(intl::format("UNSUPPORTED_LANGUAGE", name));
        }
    };

    auto get_subitem = [&](small_int idx) {
        auto &var = m_stack.top();
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
    };

    switch (cmd.command()) {
    case opcode::NOP:
    case opcode::LABEL:         break;
    case opcode::BOXNAME:       m_box_name = cmd.get_args<opcode::BOXNAME>().comment; break;
    case opcode::COMMENT:       m_last_line = cmd.get_args<opcode::COMMENT>(); break;
    case opcode::NEWBOX:        m_current_box = {}; break;
    case opcode::MVBOX:         move_box(cmd.get_args<opcode::MVBOX>(), m_stack.pop()); break;
    case opcode::MVNBOX:        move_box(cmd.get_args<opcode::MVNBOX>(), -m_stack.pop()); break;
    case opcode::RDBOX:         read_box(cmd.get_args<opcode::RDBOX>()); break;
    case opcode::SELVAR:        m_selected.emplace_back(cmd.get_args<opcode::SELVAR>(), *m_current_table); break;
    case opcode::SELVARDYN:     m_selected.emplace_back(m_stack.pop().as_string(), *m_current_table); break;
    case opcode::SELGLOBAL:     m_selected.emplace_back(cmd.get_args<opcode::SELGLOBAL>(), m_globals); break;
    case opcode::SELGLOBALDYN:  m_selected.emplace_back(m_stack.pop().as_string(), m_globals); break;
    case opcode::SELLOCAL:      m_selected.emplace_back(cmd.get_args<opcode::SELLOCAL>(), m_calls.top().vars); break;
    case opcode::SELLOCALDYN:   m_selected.emplace_back(m_stack.pop().as_string(), m_calls.top().vars); break;
    case opcode::SELINDEX:      m_selected.top().add_index(cmd.get_args<opcode::SELINDEX>()); break;
    case opcode::SELINDEXDYN:   m_selected.top().add_index(m_stack.pop().as_int()); break;
    case opcode::SELSIZE:       m_selected.top().set_size(cmd.get_args<opcode::SELSIZE>()); break;
    case opcode::SELSIZEDYN:    m_selected.top().set_size(m_stack.pop().as_int()); break;
    case opcode::SELAPPEND:     m_selected.top().add_append(); break;
    case opcode::SELEACH:       m_selected.top().add_each(); break;
    case opcode::SETVAR:        m_selected.pop().set_value(m_stack.pop()); break;
    case opcode::FORCEVAR:      m_selected.pop().force_value(m_stack.pop()); break;
    case opcode::INCVAR:        m_selected.pop().inc_value(m_stack.pop()); break;
    case opcode::DECVAR:        m_selected.pop().dec_value(m_stack.pop()); break;
    case opcode::CLEAR:         m_selected.pop().clear_value(); break;
    case opcode::SUBITEM:       get_subitem(cmd.get_args<opcode::SUBITEM>()); break;
    case opcode::SUBITEMDYN:    get_subitem(m_stack.pop().as_int()); break;
    case opcode::PUSHVAR:       m_stack.push(m_selected.pop().get_value()); break;
    case opcode::PUSHVIEW:      m_stack.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:       m_stack.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHINT:       m_stack.push(cmd.get_args<opcode::PUSHINT>()); break;
    case opcode::PUSHDOUBLE:    m_stack.push(cmd.get_args<opcode::PUSHDOUBLE>()); break;
    case opcode::PUSHSTR:       m_stack.push(cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHREGEX:     m_stack.emplace(cmd.get_args<opcode::PUSHREGEX>(), as_regex_tag); break;
    case opcode::CALL:          m_stack.push(call_function(cmd.get_args<opcode::CALL>())); break;
    case opcode::SYSCALL:       call_function(cmd.get_args<opcode::SYSCALL>()); break;
    case opcode::CNTADD:        m_contents.emplace(m_stack.pop()); break;
    case opcode::CNTADDLIST:    m_contents.emplace(m_stack.pop(), as_array_tag); break;
    case opcode::CNTPOP:        m_contents.pop_back(); break;
    case opcode::NEXTRESULT:    m_contents.top().nextresult(); break;
    case opcode::JMP:           jump_to(cmd.get_args<opcode::JMP>()); break;
    case opcode::JZ:            if(!m_stack.pop().is_true()) jump_to(cmd.get_args<opcode::JZ>()); break;
    case opcode::JNZ:           if(m_stack.pop().is_true()) jump_to(cmd.get_args<opcode::JNZ>()); break;
    case opcode::JTE:           if(m_contents.top().tokenend()) jump_to(cmd.get_args<opcode::JTE>()); break;
    case opcode::JSRVAL:        jump_subroutine(cmd.get_args<opcode::JSRVAL>(), true); break;
    case opcode::JSR:           jump_subroutine(cmd.get_args<opcode::JSR>()); break;
    case opcode::RET:           jump_return(); break;
    case opcode::RETVAL:        jump_return_value(); break;
    case opcode::IMPORT:        import_layout(cmd.get_args<opcode::IMPORT>()); break;
    case opcode::ADDLAYOUT:     push_layout(cmd.get_args<opcode::ADDLAYOUT>()); break;
    case opcode::SETCURLAYOUT:  m_current_layout = m_layouts.begin() + cmd.get_args<opcode::SETCURLAYOUT>(); break;
    case opcode::SETLAYOUT:     if (m_flags.check(reader_flags::HALT_ON_SETLAYOUT)) m_running = false; break;
    case opcode::SETLANG:       set_language(cmd.get_args<opcode::SETLANG>()); break;
    }
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