#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"
#include "binary_bls.h"

void reader::clear() {
    m_code.clear();
    m_flags = 0;
}

void reader::start() {
    m_values.clear();
    m_notes.clear();
    m_layouts.clear();
    m_stack.clear();
    m_contents.clear();

    m_selected = {};
    m_current_box = {};

    m_program_counter = 0;
    m_table_index = 0;

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
    auto select_var = [&](variable_selector sel) {
        if (sel.flags & selvar_flags::DYN_LEN) {
            sel.length = m_stack.pop().as_int();
        }
        if (sel.flags & selvar_flags::DYN_IDX) {
            sel.index = m_stack.pop().as_int();
        }

        m_selected = variable_ref(m_values, variable_key{
            (sel.flags & selvar_flags::DYN_NAME)
                ? m_stack.pop().as_string() : *sel.name,
            (sel.flags & selvar_flags::GLOBAL)
                ? variable_key::global_index : m_table_index},
            sel.index, sel.length);

        if (sel.flags & selvar_flags::EACH) {
            m_selected.index = 0;
            m_selected.length = m_selected.size();
        }
        if (sel.flags & selvar_flags::APPEND) {
            m_selected.index = m_selected.size();
        }
    };

    auto jump_to = [&](const jump_address &label) {
        m_program_counter += std::get<ptrdiff_t>(label);
    };

    auto jump_subroutine = [&](const jsr_address &address, bool nodiscard = false) {
        m_calls.push(function_call{m_program_counter, small_int(m_stack.size() - address.numargs), address.numargs, nodiscard});
        jump_to(address);
    };

    auto push_function_arg = [&](small_int idx) {
        m_stack.push(m_stack[m_calls.top().first_arg + idx]);
    };

    auto jump_return = [&](auto &&ret_value) {
        auto fun_call = m_calls.pop();
        m_program_counter = fun_call.return_addr;
        m_stack.resize(m_stack.size() - fun_call.numargs);
        if (fun_call.nodiscard) {
            m_stack.push(std::forward<decltype(ret_value)>(ret_value));
        }
    };

    auto move_box = [&](spacer_index idx) {
        auto amt = m_stack.pop();
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

    auto get_doc_info = [&](doc_index idx) -> variable {
        switch (idx) {
        case doc_index::NPAGES:
            return m_doc->num_pages();
        case doc_index::ATE:
            return m_current_box.page > m_doc->num_pages();
        default:
            return variable();
        }
    };

    auto read_box = [&](readbox_options opts) {
        m_current_box.mode = opts.mode;
        m_current_box.type = opts.type;
        m_current_box.flags = opts.flags;

        m_contents.push(m_doc->get_text(m_current_box));
    };

    auto call_function = [&](const command_call &cmd) {
        variable ret = cmd.fun->second(arg_list(
            m_stack.end() - cmd.numargs,
            m_stack.end()));
        m_stack.resize(m_stack.size() - cmd.numargs);
        return ret;
    };

    auto import_layout = [&](const std::filesystem::path &filename) {
        jsr_address addr{ptrdiff_t(add_layout(filename) - m_program_counter), 0};
        m_code[m_program_counter] = make_command<opcode::JSR>(addr);
        jump_subroutine(addr);
    };

    auto push_layout = [&](const std::filesystem::path &filename) {
        m_layouts.push_back(filename);
        m_code[m_program_counter] = make_command<opcode::NOP>();
    };

    auto throw_error = [&]() {
        m_running = false;
        auto message = m_stack.pop().as_string();
        auto errcode = m_stack.pop().as_int();
        throw layout_runtime_error(message, errcode);
    };

    switch (cmd.command()) {
    case opcode::NOP:
    case opcode::COMMENT:
    case opcode::LABEL:      break;
    case opcode::NEWBOX:     m_current_box = {}; break;
    case opcode::MVBOX:      move_box(cmd.get_args<opcode::MVBOX>()); break;
    case opcode::RDBOX:      read_box(cmd.get_args<opcode::RDBOX>()); break;
    case opcode::NEXTTABLE:  ++m_table_index; break;
    case opcode::SELVAR:     select_var(cmd.get_args<opcode::SELVAR>()); break;
    case opcode::SETVAR:     m_selected.set_value(m_stack.pop(), cmd.get_args<opcode::SETVAR>()); break;
    case opcode::CLEAR:      m_selected.clear_value(); break;
    case opcode::GETSIZE:    m_stack.push(m_selected.size()); break;
    case opcode::PUSHVAR:    m_stack.push(m_selected.get_value()); break;
    case opcode::PUSHREF:    m_stack.push(m_selected.get_value().as_view()); break;
    case opcode::PUSHVIEW:   m_stack.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:    m_stack.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHINT:    m_stack.push(cmd.get_args<opcode::PUSHINT>()); break;
    case opcode::PUSHDOUBLE: m_stack.push(cmd.get_args<opcode::PUSHDOUBLE>()); break;
    case opcode::PUSHSTR:    m_stack.push(*cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHARG:    push_function_arg(cmd.get_args<opcode::PUSHARG>()); break;
    case opcode::GETBOX:     m_stack.push(get_box_info(cmd.get_args<opcode::GETBOX>())); break;
    case opcode::GETDOC:     m_stack.push(get_doc_info(cmd.get_args<opcode::GETDOC>())); break;
    case opcode::CALL:       m_stack.push(call_function(cmd.get_args<opcode::CALL>())); break;
    case opcode::ADDCONTENT: m_contents.push(m_stack.pop()); break;
    case opcode::POPCONTENT: m_contents.pop(); break;
    case opcode::SETBEGIN:   m_contents.top().setbegin(m_stack.pop().as_int()); break;
    case opcode::SETEND:     m_contents.top().setend(m_stack.pop().as_int()); break;
    case opcode::NEWVIEW:    m_contents.top().newview(); break;
    case opcode::SPLITVIEW:  m_contents.top().splitview(); break;
    case opcode::NEXTRESULT: m_contents.top().nextresult(); break;
    case opcode::RESETVIEW:  m_contents.top().resetview(); break;
    case opcode::JMP:        jump_to(cmd.get_args<opcode::JMP>()); break;
    case opcode::JZ:         if(!m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JZ>()); break;
    case opcode::JNZ:        if(m_stack.pop().as_bool()) jump_to(cmd.get_args<opcode::JNZ>()); break;
    case opcode::JNTE:       if(!m_contents.top().tokenend()) jump_to(cmd.get_args<opcode::JNTE>()); break;
    case opcode::JSRVAL:     jump_subroutine(cmd.get_args<opcode::JSRVAL>(), true); break;
    case opcode::JSR:        jump_subroutine(cmd.get_args<opcode::JSR>()); break;
    case opcode::THROWERROR: throw_error(); break;
    case opcode::ADDNOTE:    m_notes.push_back(m_stack.pop().as_string()); break;
    case opcode::RET:        jump_return(variable()); break;
    case opcode::RETVAL:     jump_return(m_stack.pop()); break;
    case opcode::RETVAR:     jump_return(m_selected.get_value()); break;
    case opcode::IMPORT:     import_layout(*cmd.get_args<opcode::IMPORT>()); break;
    case opcode::LAYOUTNAME: push_layout(*cmd.get_args<opcode::LAYOUTNAME>()); break;
    case opcode::SETLAYOUT:  if (m_flags & reader_flags::HALT_ON_SETLAYOUT) m_running = false; break;
    case opcode::HLT:        m_running = false; break;
    }
}

size_t reader::add_layout(const std::filesystem::path &filename) {
    std::filesystem::path cache_filename = filename;
    cache_filename.replace_extension(".cache");

    auto is_modified = [&](const std::filesystem::path &file) {
        return std::filesystem::exists(file) && std::filesystem::last_write_time(file) > std::filesystem::last_write_time(cache_filename);
    };
    
    bytecode new_code;
    auto recompile = [&] {
        parser my_parser;
        if (m_flags & reader_flags::RECURSIVE) {
            my_parser.add_flags(parser_flags::RECURSIVE_IMPORTS);
        }
        my_parser.read_layout(filename, layout_box_list::from_file(filename));
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
            return line.command() == opcode::LAYOUTNAME && is_modified(*line.get_args<opcode::LAYOUTNAME>());
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

    m_code.reserve(m_code.size() + 1 + new_code.size());
    m_code.add_line<opcode::NOP>();

    std::ranges::move(new_code, std::back_inserter(m_code));
    if (addr == 0) {
        m_code.add_line<opcode::HLT>();
    } else {
        m_code.add_line<opcode::RET>();
    }
    
    return addr;
}