#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"
#include "binary_bls.h"
#include "intl.h"

void reader::clear() {
    m_code.clear();
    m_values.clear();
    m_warnings.clear();
    m_layouts.clear();
    m_table_index = 0;
    m_flags = 0;
}

void reader::start() {
    m_stack.clear();
    m_contents.clear();

    m_selected = {};
    m_current_box = {};

    m_program_counter = 0;
    m_jumped = false;

    m_running = true;

    while (m_running && m_program_counter < m_code.size()) {
        exec_command(m_code[m_program_counter]);
        if (!m_jumped) {
            ++m_program_counter;
        } else {
            m_jumped = false;
        }
    }
    
    intl::reset_language();
    m_running = false;
}

void reader::exec_command(const command_args &cmd) {
    auto select_var = [&](const variable_selector &sel) {
        m_selected = variable_ref(m_values,
            variable_key{sel.name, (sel.flags & selvar_flags::GLOBAL) ? variable_key::global_index : m_table_index},
            sel.index, sel.length);

        if (sel.flags & selvar_flags::DYN_LEN) {
            m_selected.length = m_stack.pop().as_int();
        }
        if (sel.flags & selvar_flags::DYN_IDX) {
            m_selected.index = m_stack.pop().as_int();
        }
        if (sel.flags & selvar_flags::EACH) {
            m_selected.index = 0;
            m_selected.length = m_selected.size();
        }
        if (sel.flags & selvar_flags::APPEND) {
            m_selected.index = m_selected.size();
        }
    };

    auto jump_relative = [&](jump_address address) {
        m_program_counter += address.relative_addr;
        m_jumped = true;
    };

    auto jump_conditional = [&](jump_address address, bool condition) {
        m_program_counter += address.relative_addr & -condition;
        m_jumped = condition;
    };

    auto jump_subroutine = [&](jsr_address address, bool nodiscard = false) {
        m_calls.push(function_call{m_program_counter, small_int(m_stack.size() - address.numargs), address.numargs, nodiscard});
        jump_relative(address);
    };

    auto get_function_arg = [&](small_int idx) {
        auto ret_value = m_calls.top();
        if (idx < ret_value.numargs) {
            return m_stack[m_calls.top().first_arg + idx];
        } else {
            return variable();
        }
    };

    auto jump_return = [&](bool get_value) {
        variable ret_value;
        if (get_value) {
            ret_value = m_stack.pop();
        }
        auto fun_call = m_calls.pop();
        m_program_counter = fun_call.return_addr;
        m_stack.resize(m_stack.size() - fun_call.numargs);
        if (fun_call.nodiscard) {
            m_stack.push(ret_value);
        }
    };

    auto move_box = [&](spacer_index idx) {
        auto amt = m_stack.pop();
        switch (idx) {
        case spacer_index::PAGE:
            m_current_box.page += amt.as_int(); break;
        case spacer_index::ROTATE_CW:
            m_current_box.rotate(amt.as_int()); break;
        case spacer_index::ROTATE_CCW:
            m_current_box.rotate(-amt.as_int()); break;
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

    auto get_box_info = [&](spacer_index idx) -> variable {
        switch (idx) {
        case spacer_index::PAGE:
            return m_current_box.page;
        case spacer_index::X:
        case spacer_index::LEFT:
            return m_current_box.x;
        case spacer_index::Y:
        case spacer_index::TOP:
            return m_current_box.y;
        case spacer_index::WIDTH:
            return m_current_box.w;
        case spacer_index::HEIGHT:
            return m_current_box.h;
        case spacer_index::RIGHT:
            return m_current_box.x + m_current_box.w;
        case spacer_index::BOTTOM:
            return m_current_box.y + m_current_box.h;
        default:
            return variable();
        }
    };

    auto call_function = [&](const command_call &cmd) {
        variable ret = cmd.fun->second(arg_list(
            m_stack.end() - cmd.numargs,
            m_stack.end()));
        m_stack.resize(m_stack.size() - cmd.numargs);
        return ret;
    };

    auto import_layout = [&](const import_options &args) {
        if (args.flags & import_flags::SETLAYOUT && m_flags & reader_flags::HALT_ON_SETLAYOUT) {
            m_layouts.push_back(args.filename);
            halt();
        } else if (args.flags & import_flags::NOIMPORT) {
            if (std::ranges::find(m_layouts, args.filename) == m_layouts.end()) {
                m_layouts.push_back(args.filename);
            }
        } else {
            jsr_address addr{add_layout(args.filename) - m_program_counter, 0};
            m_code[m_program_counter] = make_command<opcode::JSR>(addr);
            jump_subroutine(addr);
        }
    };

    switch (cmd.command()) {
    case opcode::NOP:        break;
    case opcode::SETBOX:     m_contents.clear(); m_current_box = cmd.get_args<opcode::SETBOX>(); break;
    case opcode::MVBOX:      move_box(cmd.get_args<opcode::MVBOX>()); break;
    case opcode::RDBOX:      m_contents.push(m_doc->get_text(m_current_box)); break;
    case opcode::NEXTTABLE:  ++m_table_index; break;
    case opcode::SELVAR:     select_var(cmd.get_args<opcode::SELVAR>()); break;
    case opcode::SETVAR:     m_selected.set_value(m_stack.pop(), cmd.get_args<opcode::SETVAR>()); break;
    case opcode::CLEAR:      m_selected.clear(); break;
    case opcode::ISSET:      m_stack.push(m_selected.size() != 0); break;
    case opcode::GETSIZE:    m_stack.push(m_selected.size()); break;
    case opcode::PUSHVAR:    m_stack.push(m_selected.get_value()); break;
    case opcode::PUSHREF:    m_stack.push(m_selected.get_value().as_view()); break;
    case opcode::PUSHVIEW:   m_stack.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:    m_stack.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHINT:    m_stack.push(cmd.get_args<opcode::PUSHINT>()); break;
    case opcode::PUSHSTR:    m_stack.push(cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHARG:    m_stack.push(get_function_arg(cmd.get_args<opcode::PUSHARG>())); break;
    case opcode::GETBOX:     m_stack.push(get_box_info(cmd.get_args<opcode::GETBOX>())); break;
    case opcode::DOCPAGES:   m_stack.push(m_doc->num_pages()); break;
    case opcode::ATE:        m_stack.push(m_current_box.page > m_doc->num_pages()); break;
    case opcode::CALL:       m_stack.push(call_function(cmd.get_args<opcode::CALL>())); break;
    case opcode::ADDCONTENT: m_contents.push(m_stack.pop()); break;
    case opcode::POPCONTENT: m_contents.pop(); break;
    case opcode::SETBEGIN:   m_contents.top().setbegin(m_stack.pop().as_int()); break;
    case opcode::SETEND:     m_contents.top().setend(m_stack.pop().as_int()); break;
    case opcode::NEWVIEW:    m_contents.top().newview(); break;
    case opcode::SPLITVIEW:  m_contents.top().splitview(); break;
    case opcode::NEXTRESULT: m_contents.top().nextresult(); break;
    case opcode::RESETVIEW:  m_contents.top().resetview(); break;
    case opcode::IMPORT:     import_layout(cmd.get_args<opcode::IMPORT>()); break;
    case opcode::SETLANG:    intl::set_language(cmd.get_args<opcode::SETLANG>()); break;
    case opcode::JMP:        jump_relative(cmd.get_args<opcode::JMP>()); break;
    case opcode::JZ:         jump_conditional(cmd.get_args<opcode::JZ>(), !m_stack.pop().as_bool()); break;
    case opcode::JNZ:        jump_conditional(cmd.get_args<opcode::JNZ>(), m_stack.pop().as_bool()); break;
    case opcode::JNTE:       jump_conditional(cmd.get_args<opcode::JNTE>(), !m_contents.top().tokenend()); break;
    case opcode::JSRVAL:     jump_subroutine(cmd.get_args<opcode::JSRVAL>(), true); break;
    case opcode::JSR:        jump_subroutine(cmd.get_args<opcode::JSR>()); break;
    case opcode::THROWERROR: throw layout_error(m_stack.pop().as_string()); break;
    case opcode::WARNING:    m_warnings.push_back(m_stack.pop().as_string()); break;
    case opcode::RET:        jump_return(false); break;
    case opcode::RETVAL:     jump_return(true); break;
    case opcode::HLT:        halt(); break;
    }
}

size_t reader::add_layout(const std::filesystem::path &filename) {
    m_layouts.push_back(filename);

    std::filesystem::path cache_filename = filename;
    cache_filename.replace_extension(".cache");

    auto is_modified = [&](const std::filesystem::path &file) {
        return std::filesystem::last_write_time(file) > std::filesystem::last_write_time(cache_filename);
    };
    
    bytecode new_code;
    auto recompile = [&] {
        parser my_parser;
        if (m_flags & reader_flags::RECURSIVE) {
            my_parser.add_flags(parser_flags::RECURSIVE_IMPORTS);
        }
        my_parser.read_layout(filename.parent_path(), box_vector::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags & reader_flags::USE_CACHE) {
            binary_bls::write(new_code, cache_filename);
        }
    };

    // Se settata flag USE_CACHE, leggi il file di cache e
    // ricompila solo se uno dei file importati è stato modificato.
    // Altrimenti ricompila sempre
    if (m_flags & reader_flags::USE_CACHE && std::filesystem::exists(cache_filename) && !is_modified(filename)) {
        new_code = binary_bls::read(cache_filename);
        if (m_flags & reader_flags::RECURSIVE && std::ranges::any_of(new_code, [&](const command_args &line) {
            return line.command() == opcode::IMPORT && is_modified(line.get_args<opcode::IMPORT>().filename);
        })) {
            recompile();
        }
    } else {
        recompile();
    }

    return add_code(std::move(new_code));
}

size_t reader::add_code(bytecode &&new_code) {
    size_t addr = m_code.size();

    std::ranges::move(new_code, std::back_inserter(m_code));
    if (addr == 0) {
        m_code.push_back(make_command<opcode::HLT>());
    } else {
        m_code.push_back(make_command<opcode::RET>());
    }
    
    return addr;
}