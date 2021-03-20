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
            variable_key{sel.name, (sel.flags & SEL_GLOBAL) ? variable_key::global_index : m_table_index},
            sel.index, sel.length);

        if (sel.flags & SEL_DYN_LEN) {
            m_selected.length = m_stack.pop().as_int();
        }
        if (sel.flags & SEL_DYN_IDX) {
            m_selected.index = m_stack.pop().as_int();
        }
        if (sel.flags & SEL_EACH) {
            m_selected.index = 0;
            m_selected.length = m_selected.size();
        }
        if (sel.flags & SEL_APPEND) {
            m_selected.index = m_selected.size();
        }
    };

    auto jump_relative = [&](jump_address address) {
        m_program_counter += address;
        m_jumped = true;
    };

    auto jump_conditional = [&](jump_address address, bool condition) {
        m_program_counter += address & -condition;
        m_jumped = condition;
    };

    auto jump_subroutine = [&](jump_address address) {
        m_stack.push(m_program_counter);
        jump_relative(address);
    };

    auto move_box = [&](spacer_index idx) {
        auto amt = m_stack.pop();
        switch (idx) {
        case SPACER_PAGE:
            m_current_box.page += amt.as_int(); break;
        case SPACER_ROTATE_CW:
            m_current_box.rotate(amt.as_int()); break;
        case SPACER_ROTATE_CCW:
            m_current_box.rotate(-amt.as_int()); break;
        case SPACER_X:
            m_current_box.x += amt.as_double(); break;
        case SPACER_Y:
            m_current_box.y += amt.as_double(); break;
        case SPACER_WIDTH:
        case SPACER_RIGHT:
            m_current_box.w += amt.as_double(); break;
        case SPACER_HEIGHT:
        case SPACER_BOTTOM:
            m_current_box.h += amt.as_double(); break;
        case SPACER_TOP:
            m_current_box.y += amt.as_double();
            m_current_box.h -= amt.as_double();
            break;
        case SPACER_LEFT:
            m_current_box.x += amt.as_double();
            m_current_box.w -= amt.as_double();
            break;
        }
    };

    auto get_box_info = [&](spacer_index idx) -> variable {
        switch (idx) {
        case SPACER_PAGE:
            return m_current_box.page;
        case SPACER_X:
        case SPACER_LEFT:
            return m_current_box.x;
        case SPACER_Y:
        case SPACER_TOP:
            return m_current_box.y;
        case SPACER_WIDTH:
            return m_current_box.w;
        case SPACER_HEIGHT:
            return m_current_box.h;
        case SPACER_RIGHT:
            return m_current_box.x + m_current_box.w;
        case SPACER_BOTTOM:
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
        m_stack.push(std::move(ret));
    };

    auto import_layout = [&](const import_options &args) {
        if (args.flags & IMPORT_SETLAYOUT && m_flags & READER_HALT_ON_SETLAYOUT) {
            m_layouts.push_back(args.filename);
            halt();
        } else if (args.flags & IMPORT_IGNORE) {
            if (std::ranges::find(m_layouts, args.filename) == m_layouts.end()) {
                m_layouts.push_back(args.filename);
            }
        } else {
            jump_address addr = add_layout(args.filename) - m_program_counter;
            m_code[m_program_counter] = make_command<OP_JSR>(addr);
            jump_subroutine(addr);
        }
    };

    switch (cmd.command()) {
    case OP_NOP:        break;
    case OP_SETBOX:     m_contents.clear(); m_current_box = cmd.get_args<OP_SETBOX>(); break;
    case OP_MVBOX:      move_box(cmd.get_args<OP_MVBOX>()); break;
    case OP_RDBOX:      m_contents.push(m_doc->get_text(m_current_box)); break;
    case OP_NEXTTABLE:  ++m_table_index; break;
    case OP_SELVAR:     select_var(cmd.get_args<OP_SELVAR>()); break;
    case OP_SETVAR:     m_selected.set_value(m_stack.pop(), cmd.get_args<OP_SETVAR>()); break;
    case OP_CLEAR:      m_selected.clear(); break;
    case OP_ISSET:      m_stack.push(m_selected.size() != 0); break;
    case OP_GETSIZE:    m_stack.push(m_selected.size()); break;
    case OP_PUSHVAR:    m_stack.push(m_selected.get_value()); break;
    case OP_PUSHREF:    m_stack.push(m_selected.get_value().str_view()); break;
    case OP_PUSHVIEW:   m_stack.push(m_contents.top().view()); break;
    case OP_PUSHNUM:    m_stack.push(cmd.get_args<OP_PUSHNUM>()); break;
    case OP_PUSHINT:    m_stack.push(cmd.get_args<OP_PUSHINT>()); break;
    case OP_PUSHSTR:    m_stack.push(cmd.get_args<OP_PUSHSTR>()); break;
    case OP_GETBOX:     m_stack.push(get_box_info(cmd.get_args<OP_GETBOX>())); break;
    case OP_DOCPAGES:   m_stack.push(m_doc->num_pages()); break;
    case OP_ATE:        m_stack.push(m_current_box.page > m_doc->num_pages()); break;
    case OP_CALL:       call_function(cmd.get_args<OP_CALL>()); break;
    case OP_ADDCONTENT: m_contents.push(m_stack.pop()); break;
    case OP_POPCONTENT: m_contents.pop(); break;
    case OP_SETBEGIN:   m_contents.top().setbegin(m_stack.pop().as_int()); break;
    case OP_SETEND:     m_contents.top().setend(m_stack.pop().as_int()); break;
    case OP_NEWVIEW:    m_contents.top().newview(); break;
    case OP_SPLITVIEW:  m_contents.top().splitview(); break;
    case OP_NEXTRESULT: m_contents.top().nextresult(); break;
    case OP_RESETVIEW:  m_contents.top().resetview(); break;
    case OP_IMPORT:     import_layout(cmd.get_args<OP_IMPORT>()); break;
    case OP_SETLANG:    intl::set_language(cmd.get_args<OP_SETLANG>()); break;
    case OP_JMP:        jump_relative(cmd.get_args<OP_JMP>()); break;
    case OP_JSR:        jump_subroutine(cmd.get_args<OP_JSR>()); break;
    case OP_JZ:         jump_conditional(cmd.get_args<OP_JZ>(), !m_stack.pop().as_bool()); break;
    case OP_JNZ:        jump_conditional(cmd.get_args<OP_JNZ>(), m_stack.pop().as_bool()); break;
    case OP_JNTE:       jump_conditional(cmd.get_args<OP_JNTE>(), !m_contents.top().tokenend()); break;
    case OP_THROWERROR: throw layout_error(m_stack.pop().str()); break;
    case OP_WARNING:    m_warnings.push_back(m_stack.pop().str()); break;
    case OP_RET:        m_program_counter = m_stack.pop().as_int(); break;
    case OP_HLT:        halt(); break;
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
        if (m_flags & READER_RECURSIVE) {
            my_parser.add_flags(PARSER_RECURSIVE_IMPORTS);
        }
        my_parser.read_layout(filename.parent_path(), box_vector::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags & READER_USE_CACHE) {
            binary_bls::write(new_code, cache_filename);
        }
    };

    // Se settata flag USE_CACHE, leggi il file di cache e
    // ricompila solo se uno dei file importati è stato modificato.
    // Altrimenti ricompila sempre
    if (m_flags & READER_USE_CACHE && std::filesystem::exists(cache_filename) && !is_modified(filename)) {
        new_code = binary_bls::read(cache_filename);
        if (m_flags & READER_RECURSIVE && std::ranges::any_of(new_code, [&](const command_args &line) {
            return line.command() == OP_IMPORT && is_modified(line.get_args<OP_IMPORT>().filename);
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
        m_code.push_back(make_command<OP_HLT>());
    } else {
        m_code.push_back(make_command<OP_RET>());
    }
    
    return addr;
}