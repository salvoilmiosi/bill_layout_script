#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"
#include "binary_bls.h"
#include "intl.h"

#include <sstream>

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
    m_spacer = {};
    m_last_box = {};

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

    auto jump_subroutine = [&](size_t address) {
        fixed_point return_addr;
        return_addr.setUnbiased(m_program_counter);
        m_stack.push(return_addr);
        m_program_counter = address;
        m_jumped = true;
    };

    auto read_box = [&](pdf_rect box) {
        m_last_box.page = box.page += m_spacer.page;
        m_last_box.x = box.x += m_spacer.x;
        m_last_box.y = box.y += m_spacer.y;
        m_last_box.w = box.w += m_spacer.w;
        m_last_box.h = box.h += m_spacer.h;
        m_last_box.rotation = m_spacer.rotation;

        m_contents.clear();
        m_contents.push(m_doc->get_text(box, m_spacer.rotation));
        m_spacer = {};
    };

    auto call_function = [&](const command_call &cmd) {
        variable ret = cmd.fun->second(arg_list(
            m_stack.end() - cmd.numargs,
            m_stack.end()));
        m_stack.resize(m_stack.size() - cmd.numargs);
        m_stack.push(std::move(ret));
    };

    switch(cmd.command()) {
    case OP_NOP: break;
    case OP_RDBOX:      read_box(cmd.get_args<OP_RDBOX>()); break;
    case OP_CALL:       call_function(cmd.get_args<OP_CALL>()); break;
    case OP_THROWERROR: throw layout_error(m_stack.top().str()); break;
    case OP_WARNING:    m_warnings.push_back(std::move(m_stack.pop().str())); break;
    case OP_SELVAR: select_var(cmd.get_args<OP_SELVAR>()); break;
    case OP_ISSET: m_stack.push(m_selected.size() != 0); break;
    case OP_GETSIZE: m_stack.push(m_selected.size()); break;
    case OP_MVBOX: {
        auto amt = m_stack.pop();
        auto add_rotation = [&](int amt) {
            m_spacer.rotation += amt;
            while (amt > 0 && m_spacer.rotation >= 4) {
                m_spacer.rotation -= 4;
            }
            while (amt < 0 && m_spacer.rotation < 0) {
                m_spacer.rotation += 4;
            }
        };
        switch (cmd.get_args<OP_MVBOX>()) {
        case SPACER_PAGE:
            m_spacer.page += amt.as_int(); break;
        case SPACER_ROTATE_CW:
            add_rotation(amt.as_int()); break;
        case SPACER_ROTATE_CCW:
            add_rotation(-amt.as_int()); break;
        case SPACER_X:
            m_spacer.x += amt.as_double(); break;
        case SPACER_Y:
            m_spacer.y += amt.as_double(); break;
        case SPACER_WIDTH:
        case SPACER_RIGHT:
            m_spacer.w += amt.as_double(); break;
        case SPACER_HEIGHT:
        case SPACER_BOTTOM:
            m_spacer.h += amt.as_double(); break;
        case SPACER_TOP:
            m_spacer.y += amt.as_double();
            m_spacer.h -= amt.as_double();
            break;
        case SPACER_LEFT:
            m_spacer.x += amt.as_double();
            m_spacer.w -= amt.as_double();
            break;
        }
        break;
    }
    case OP_GETBOX:
        switch (cmd.get_args<OP_GETBOX>()) {
        case SPACER_PAGE:
            m_stack.push(m_last_box.page); break;
        case SPACER_ROTATE_CW:
            m_stack.push(m_last_box.rotation); break;
        case SPACER_ROTATE_CCW:
            m_stack.push((4 - m_last_box.rotation) % 4); break;
        case SPACER_X:
        case SPACER_LEFT:
            m_stack.push(m_last_box.x); break;
        case SPACER_Y:
        case SPACER_TOP:
            m_stack.push(m_last_box.y); break;
        case SPACER_WIDTH:
            m_stack.push(m_last_box.w); break;
        case SPACER_HEIGHT:
            m_stack.push(m_last_box.h); break;
        case SPACER_RIGHT:
            m_stack.push(m_last_box.x + m_last_box.w); break;
        case SPACER_BOTTOM:
            m_stack.push(m_last_box.y + m_last_box.h); break;
        }
        break;
    case OP_DOCPAGES:  m_stack.push(m_doc->num_pages()); break;
    case OP_ATE:       m_stack.push(m_last_box.page > m_doc->num_pages()); break;
    case OP_CLEAR:     m_selected.clear(); break;
    case OP_SETVAR:    m_selected.set_value(m_stack.pop(), cmd.get_args<OP_SETVAR>()); break;
    case OP_PUSHVIEW:  m_stack.push(m_contents.top().view()); break;
    case OP_PUSHNUM:   m_stack.push(cmd.get_args<OP_PUSHNUM>()); break;
    case OP_PUSHSTR:   m_stack.push(cmd.get_args<OP_PUSHSTR>()); break;
    case OP_PUSHVAR:   m_stack.push(m_selected.get_value()); break;
    case OP_PUSHREF:   m_stack.push(m_selected.get_value().str_view()); break;
    case OP_JMP:
        m_program_counter += cmd.get_args<OP_JMP>();
        m_jumped = true;
        break;
    case OP_JSR:
        jump_subroutine(m_program_counter + cmd.get_args<OP_JSR>());
        break;
    case OP_JZ:
        if (!m_stack.pop().as_bool()) {
            m_program_counter += cmd.get_args<OP_JZ>();
            m_jumped = true;
        }
        break;
    case OP_JNZ:
        if (m_stack.pop().as_bool()) {
            m_program_counter += cmd.get_args<OP_JNZ>();
            m_jumped = true;
        }
        break;
    case OP_JTE:
        if (m_contents.top().token_end()) {
            m_program_counter += cmd.get_args<OP_JTE>();
            m_jumped = true;
        }
        break;
    case OP_ADDCONTENT: m_contents.push(m_stack.pop()); break;
    case OP_POPCONTENT: m_contents.pop(); break;
    case OP_SETBEGIN:  m_contents.top().setbegin(m_stack.pop().as_int()); break;
    case OP_SETEND:    m_contents.top().setend(m_stack.pop().as_int()); break;
    case OP_NEWVIEW: m_contents.top().new_view(); break;
    case OP_SUBVIEW: m_contents.top().new_subview(); break;
    case OP_RESETVIEW: m_contents.top().reset_view(); break;
    case OP_NEXTRESULT: m_contents.top().next_result(); break;
    case OP_NEXTTABLE: ++m_table_index; break;
    case OP_RET: m_program_counter = m_stack.pop().number().getUnbiased(); break;
    case OP_HLT: halt(); break;
    case OP_IMPORT: {
        const auto &args = cmd.get_args<OP_IMPORT>();
        if (args.flags & IMPORT_SETLAYOUT && m_flags & READER_HALT_ON_SETLAYOUT) {
            m_layouts.push_back(args.filename);
            halt();
        } else if (args.flags & IMPORT_IGNORE) {
            if (std::ranges::find(m_layouts, args.filename) == m_layouts.end()) {
                m_layouts.push_back(args.filename);
            }
        } else {
            size_t addr = add_layout(args.filename);
            m_code[m_program_counter] = make_command<OP_JSR>(jump_address(addr - m_program_counter));
            jump_subroutine(addr);
        }
        break;
    }
    case OP_SETLANG: intl::set_language(cmd.get_args<OP_SETLANG>()); break;
    }
}

size_t reader::add_layout(const std::filesystem::path &filename) {
    m_layouts.push_back(filename);

    std::filesystem::path cache_filename = filename;
    cache_filename.replace_extension(".cache");
    
    bytecode new_code;
    // Se settata flag USE_CACHE, leggi il file di cache e
    // ricompila solo se uno dei file importati è stato modificato.
    // Altrimenti ricompila sempre
    if (!(m_flags & READER_USE_CACHE && std::filesystem::exists(cache_filename))
        || std::filesystem::last_write_time(filename) > std::filesystem::last_write_time(cache_filename)
        || std::ranges::any_of(new_code = binary_bls::read(cache_filename), [&](const command_args &line) {
            return line.command() == OP_IMPORT
                && std::filesystem::last_write_time(line.get_args<OP_IMPORT>().filename)
                > std::filesystem::last_write_time(cache_filename);
        })) {
        parser my_parser;
        if (m_flags & READER_RECURSIVE) {
            my_parser.add_flags(PARSER_RECURSIVE_IMPORTS);
        }
        my_parser.read_layout(filename.parent_path(), box_vector::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags & READER_USE_CACHE) {
            binary_bls::write(new_code, cache_filename);
        }
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