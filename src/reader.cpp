#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"
#include "binary_bls.h"

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
    m_vars.clear();
    m_contents.clear();
    m_return_addrs.clear();

    m_selected = {};
    m_spacer = {};
    m_last_box_page = 0;

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
    auto exec_operator = [&](auto op) {
        auto ret = op(m_vars[m_vars.size() - 2], m_vars.top());
        m_vars.resize(m_vars.size() - 2);
        m_vars.push(std::move(ret));
    };

    auto select_var = [&](const variable_selector &sel) {
        m_selected = variable_ref(m_values,
            variable_key{sel.name, (sel.flags & SEL_GLOBAL) ? variable_key::global_index : m_table_index},
            sel.index, sel.length);

        if (sel.flags & SEL_DYN_LEN) {
            m_selected.length = m_vars.pop().as_int();
        }

        if (sel.flags & SEL_DYN_IDX) {
            m_selected.index = m_vars.pop().as_int();
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
        m_return_addrs.push(m_program_counter);
        m_program_counter = address;
        m_jumped = true;
    };

    auto parse_num = [](fixed_point &num, const std::string &str) {
        std::istringstream iss(str);
        return dec::fromStream(iss, dec::decimal_format(intl::decimal_point(), intl::thousand_sep()), num);
    };

    auto read_box = [&](pdf_rect box) {
        box.page += m_spacer.page;
        box.x += m_spacer.x;
        box.y += m_spacer.y;
        box.w += m_spacer.w;
        box.h += m_spacer.h;
        m_spacer = {};

        m_last_box_page = box.page;

        m_contents = {};

        m_contents.push(m_doc->get_text(box));
    };

    auto call_function = [&](const command_call &cmd) {
        variable ret = find_function(cmd.name)(arg_list(
            m_vars.end() - cmd.numargs,
            m_vars.end()));
        m_vars.resize(m_vars.size() - cmd.numargs);
        m_vars.push(std::move(ret));
    };

    switch(cmd.command()) {
    case OP_NOP: break;
    case OP_RDBOX:      read_box(cmd.get_args<OP_RDBOX>()); break;
    case OP_CALL:       call_function(cmd.get_args<OP_CALL>()); break;
    case OP_THROWERROR: throw layout_error(m_vars.top().str()); break;
    case OP_WARNING:    m_warnings.push_back(std::move(m_vars.pop().str())); break;
    case OP_PARSEINT:   m_vars.top() = m_vars.top().as_int(); break;
    case OP_PARSENUM: {
        auto &var = m_vars.top();
        if (var.type() == VAR_STRING) {
            fixed_point num;
            if (parse_num(num, var.str())) {
                var = num;
            } else {
                var = variable::null_var();
            }
        }
        break;
    }
    case OP_AGGREGATE: {
        if (! m_vars.top().empty()) {
            fixed_point ret(0);
            content_view view(m_vars.top().str());
            for (view.new_subview(); !view.token_end(); view.next_result()) {
                fixed_point num;
                if (parse_num(num, std::string(view.view()))) {
                    ret += num;
                }
            }
            m_vars.top() = ret;
        } else {
            m_vars.top() = variable::null_var();
        }
        break;
    }
    case OP_NOT: m_vars.top() = !m_vars.top(); break;
    case OP_NEG: m_vars.top() = -m_vars.top(); break;
#define OP(x) exec_operator([](const auto &a, const auto &b) { return x; })
    case OP_EQ:  OP(a == b); break;
    case OP_NEQ: OP(a != b); break;
    case OP_AND: OP(a && b); break;
    case OP_OR:  OP(a || b); break;
    case OP_ADD: OP(a + b); break;
    case OP_SUB: OP(a - b); break;
    case OP_MUL: OP(a * b); break;
    case OP_DIV: OP(a / b); break;
    case OP_GT:  OP(a > b); break;
    case OP_LT:  OP(a < b); break;
    case OP_GEQ: OP(a >= b); break;
    case OP_LEQ: OP(a <= b); break;
#undef OP
    case OP_SELVAR: select_var(cmd.get_args<OP_SELVAR>()); break;
    case OP_ISSET: m_vars.push(m_selected.size() != 0); break;
    case OP_GETSIZE: m_vars.push(m_selected.size()); break;
    case OP_MVBOX: {
        auto amt = m_vars.pop();
        switch (cmd.get_args<OP_MVBOX>()) {
        case SPACER_PAGE:
            m_spacer.page += amt.as_int();
            break;
        case SPACER_X:
            m_spacer.x += amt.as_double();
            break;
        case SPACER_Y:
            m_spacer.y += amt.as_double();
            break;
        case SPACER_W:
        case SPACER_RIGHT:
            m_spacer.w += amt.as_double();
            break;
        case SPACER_H:
        case SPACER_BOTTOM:
            m_spacer.h += amt.as_double();
            break;
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
    case OP_CLEAR:     m_selected.clear(); break;
    case OP_SETVAR:    m_selected.set_value(m_vars.pop(), cmd.get_args<OP_SETVAR>()); break;
    case OP_PUSHVIEW:  m_vars.push(m_contents.top().view()); break;
    case OP_PUSHNUM:   m_vars.push(cmd.get_args<OP_PUSHNUM>()); break;
    case OP_PUSHSTR:   m_vars.push(cmd.get_args<OP_PUSHSTR>()); break;
    case OP_PUSHNULL:  m_vars.push(variable::null_var()); break;
    case OP_PUSHVAR:   m_vars.push(m_selected.get_value()); break;
    case OP_MOVEVAR:   m_vars.push(m_selected.get_moved()); break;
    case OP_JMP:
        m_program_counter += cmd.get_args<OP_JMP>();
        m_jumped = true;
        break;
    case OP_JSR:
        jump_subroutine(m_program_counter + cmd.get_args<OP_JSR>());
        break;
    case OP_JZ:
        if (!m_vars.pop().as_bool()) {
            m_program_counter += cmd.get_args<OP_JZ>();
            m_jumped = true;
        }
        break;
    case OP_JNZ:
        if (m_vars.pop().as_bool()) {
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
    case OP_MOVCONTENT: m_contents.push(std::move(m_vars.pop().str())); break;
    case OP_POPCONTENT: m_contents.pop(); break;
    case OP_SETBEGIN:  m_contents.top().setbegin(m_vars.pop().as_int()); break;
    case OP_SETEND:    m_contents.top().setend(m_vars.pop().as_int()); break;
    case OP_NEWVIEW: m_contents.top().new_view(); break;
    case OP_SUBVIEW: m_contents.top().new_subview(); break;
    case OP_RESETVIEW: m_contents.top().reset_view(); break;
    case OP_NEXTRESULT: m_contents.top().next_result(); break;
    case OP_NEXTTABLE: ++m_table_index; break;
    case OP_ATE: m_vars.push(m_last_box_page > m_doc->num_pages()); break;
    case OP_RET:
        if (!m_return_addrs.empty()) {
            m_program_counter = m_return_addrs.pop();
            break;
        }
        [[fallthrough]];
    case OP_HLT: halt(); break;
    case OP_IMPORT: {
        const auto &args = cmd.get_args<OP_IMPORT>();
        if (args.flags & IMPORT_SETLAYOUT && m_flags & READER_HALT_ON_SETLAYOUT) {
            m_layouts.push_back(args.filename);
            halt();
        } else if (args.flags & IMPORT_IGNORE) {
            if (std::find(m_layouts.begin(), m_layouts.end(), args.filename) == m_layouts.end()) {
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

    std::filesystem::path cache_filename;
    
    bytecode new_code;
    bool recompile = true;

    if (m_flags & READER_USE_CACHE) {
        cache_filename = filename;
        cache_filename.replace_extension(".cache");

        if (std::filesystem::exists(cache_filename)) {
            recompile = std::filesystem::last_write_time(filename) > std::filesystem::last_write_time(cache_filename);
            if (!recompile) {
                new_code = binary_bls::read(cache_filename);
                for (auto &line : new_code) {
                    if (line.command() == OP_IMPORT) {
                        auto layout_filename = line.get_args<OP_IMPORT>().filename;
                        if (std::filesystem::last_write_time(layout_filename) > std::filesystem::last_write_time(cache_filename)) {
                            recompile = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (recompile) {
        parser my_parser;
        if (m_flags & READER_RECURSIVE) {
            my_parser.add_flags(PARSER_RECURSIVE_IMPORTS);
        }
        my_parser.read_layout(filename.parent_path(), bill_layout_script::from_file(filename));
        new_code = std::move(my_parser).get_bytecode();
        if (m_flags & READER_USE_CACHE) {
            binary_bls::write(new_code, cache_filename);
        }
    }

    return add_code(std::move(new_code));
}

size_t reader::add_code(bytecode &&new_code) {
    size_t addr = m_code.size();

    std::copy(
        std::move_iterator(new_code.begin()),
        std::move_iterator(new_code.end()),
        std::back_inserter(m_code)
    );
    m_code.push_back(make_command<OP_RET>());
    
    return addr;
}