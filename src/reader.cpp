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
    m_calls.clear();
    m_selected.clear();

    m_values.emplace_back();
    m_current_table = m_values.begin();

    m_current_box = {};

    m_locale = std::locale::classic();

    m_program_counter = 0;

    m_running = true;
    m_aborted = false;

    while (m_running && m_program_counter < m_code.size()) {
        exec_command(m_code[m_program_counter]);
        ++m_program_counter;
    }
    if (m_aborted) {
        throw reader_aborted{};
    }
    
    m_running = false;
}

void reader::exec_command(const command_args &cmd) {
    auto jump_to = [&](const jump_address &label) {
        m_program_counter += label.address;
    };

    auto jump_subroutine = [&](const jsr_address &address, bool getretvalue = false) {
        m_calls.emplace(arg_list(m_stack.end() - address.numargs, m_stack.end()), m_program_counter, getretvalue);
        jump_to(address);
    };

    auto get_function_arg = [&](small_int idx) -> variable & {
        return m_calls.top().args[idx];
    };

    auto jump_return = [&](variable ret_value) {
        auto fun_call = m_calls.pop();
        m_program_counter = fun_call.return_addr;
        m_stack.resize(m_stack.size() - fun_call.args.size());
        if (fun_call.getretvalue) {
            m_stack.push(std::move(ret_value));
        }
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
        jsr_address addr{add_layout(filename) - m_program_counter, 0};
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

    switch (cmd.command()) {
    case opcode::NOP:
    case opcode::COMMENT:
    case opcode::LABEL:         break;
    case opcode::NEWBOX:        m_current_box = {}; break;
    case opcode::MVBOX:         move_box(cmd.get_args<opcode::MVBOX>(), m_stack.pop()); break;
    case opcode::MVNBOX:        move_box(cmd.get_args<opcode::MVNBOX>(), -m_stack.pop()); break;
    case opcode::RDBOX:         read_box(cmd.get_args<opcode::RDBOX>()); break;
    case opcode::SELVAR:        m_selected.emplace_back(cmd.get_args<opcode::SELVAR>(), *m_current_table); break;
    case opcode::SELVARDYN:     m_selected.emplace_back(m_stack.pop().as_string(), *m_current_table); break;
    case opcode::SELGLOBAL:     m_selected.emplace_back(cmd.get_args<opcode::SELGLOBAL>(), m_globals); break;
    case opcode::SELGLOBALDYN:  m_selected.emplace_back(m_stack.pop().as_string(), m_globals); break;
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
    case opcode::PUSHVAR:       m_stack.push(m_selected.pop().get_value()); break;
    case opcode::PUSHVIEW:      m_stack.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:       m_stack.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHINT:       m_stack.push(cmd.get_args<opcode::PUSHINT>()); break;
    case opcode::PUSHDOUBLE:    m_stack.push(cmd.get_args<opcode::PUSHDOUBLE>()); break;
    case opcode::PUSHSTR:       m_stack.push(cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHREGEX:     m_stack.emplace(regex_state{}, cmd.get_args<opcode::PUSHREGEX>()); break;
    case opcode::PUSHARG:       m_stack.push(get_function_arg(cmd.get_args<opcode::PUSHARG>()).as_pointer()); break;
    case opcode::CALL:          m_stack.push(call_function(cmd.get_args<opcode::CALL>())); break;
    case opcode::SYSCALL:       call_function(cmd.get_args<opcode::SYSCALL>()); break;
    case opcode::CNTADDSTRING:  m_contents.emplace(m_stack.pop(), false); break;
    case opcode::CNTADDLIST:    m_contents.emplace(m_stack.pop(), true); break;
    case opcode::CNTPOP:        m_contents.pop_back(); break;
    case opcode::NEXTRESULT:    m_contents.top().nextresult(); break;
    case opcode::JMP:           jump_to(cmd.get_args<opcode::JMP>()); break;
    case opcode::JZ:            if(!m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JZ>()); break;
    case opcode::JNZ:           if(m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JNZ>()); break;
    case opcode::JTE:           if(m_contents.top().tokenend()) jump_to(cmd.get_args<opcode::JTE>()); break;
    case opcode::JSRVAL:        jump_subroutine(cmd.get_args<opcode::JSRVAL>(), true); break;
    case opcode::JSR:           jump_subroutine(cmd.get_args<opcode::JSR>()); break;
    case opcode::RET:           jump_return(variable()); break;
    case opcode::RETVAL:        jump_return(m_stack.pop()); break;
    case opcode::RETARG:        jump_return(std::move(get_function_arg(cmd.get_args<opcode::RETARG>()))); break;
    case opcode::IMPORT:        import_layout(cmd.get_args<opcode::IMPORT>()); break;
    case opcode::ADDLAYOUT:     push_layout(cmd.get_args<opcode::ADDLAYOUT>()); break;
    case opcode::SETCURLAYOUT:  m_current_layout = m_layouts.begin() + cmd.get_args<opcode::SETCURLAYOUT>(); break;
    case opcode::SETLAYOUT:     if (m_flags.check(reader_flags::HALT_ON_SETLAYOUT)) m_running = false; break;
    case opcode::SETLANG:       set_language(cmd.get_args<opcode::SETLANG>()); break;
    case opcode::HLT:           m_running = false; break;
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
    size_t addr = m_code.size();

    m_code.reserve(m_code.size() + 4 + new_code.size());
    m_code.add_line<opcode::NOP>();

    std::ranges::move(new_code, std::back_inserter(m_code));
    if (addr == 0) {
        m_code.add_line<opcode::HLT>();
    } else {
        m_code.add_line<opcode::SETCURLAYOUT>(m_current_layout - m_layouts.begin());
        if (std::has_facet<boost::locale::info>(m_locale)) {
            m_code.add_line<opcode::SETLANG>(std::use_facet<boost::locale::info>(m_locale).name());
        } else {
            m_code.add_line<opcode::SETLANG>();
        }
        m_code.add_line<opcode::RET>();
    }
    
    return addr;
}