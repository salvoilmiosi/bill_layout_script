#include "reader.h"

#include "utils.h"
#include "parser.h"
#include "functions.h"

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
    case opcode::NOP: break;
    case opcode::RDBOX:         read_box(cmd.get_args<opcode::RDBOX>()); break;
    case opcode::CALL:          call_function(cmd.get_args<opcode::CALL>()); break;
    case opcode::THROWERR:      throw layout_error(m_vars.top().str()); break;
    case opcode::ADDWARNING:    m_warnings.push_back(std::move(m_vars.pop().str())); break;
    case opcode::PARSEINT: m_vars.top() = m_vars.top().as_int(); break;
    case opcode::PARSENUM: {
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
    case opcode::AGGREGATE: {
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
    case opcode::NOT: m_vars.top() = !m_vars.top(); break;
    case opcode::NEG: m_vars.top() = -m_vars.top(); break;
#define OP(x) exec_operator([](const auto &a, const auto &b) { return x; })
    case opcode::EQ:  OP(a == b); break;
    case opcode::NEQ: OP(a != b); break;
    case opcode::AND: OP(a && b); break;
    case opcode::OR:  OP(a || b); break;
    case opcode::ADD: OP(a + b); break;
    case opcode::SUB: OP(a - b); break;
    case opcode::MUL: OP(a * b); break;
    case opcode::DIV: OP(a / b); break;
    case opcode::GT:  OP(a > b); break;
    case opcode::LT:  OP(a < b); break;
    case opcode::GEQ: OP(a >= b); break;
    case opcode::LEQ: OP(a <= b); break;
#undef OP
    case opcode::SELVAR: select_var(cmd.get_args<opcode::SELVAR>()); break;
    case opcode::ISSET: m_vars.push(m_selected.size() != 0); break;
    case opcode::GETSIZE: m_vars.push(m_selected.size()); break;
    case opcode::MVBOX: {
        auto amt = m_vars.pop();
        switch (cmd.get_args<opcode::MVBOX>()) {
        case spacer_index::SPACER_PAGE:
            m_spacer.page += amt.as_int();
            break;
        case spacer_index::SPACER_X:
            m_spacer.x += amt.as_double();
            break;
        case spacer_index::SPACER_Y:
            m_spacer.y += amt.as_double();
            break;
        case spacer_index::SPACER_W:
        case spacer_index::SPACER_RIGHT:
            m_spacer.w += amt.as_double();
            break;
        case spacer_index::SPACER_H:
        case spacer_index::SPACER_BOTTOM:
            m_spacer.h += amt.as_double();
            break;
        case spacer_index::SPACER_TOP:
            m_spacer.y += amt.as_double();
            m_spacer.h -= amt.as_double();
            break;
        case spacer_index::SPACER_LEFT:
            m_spacer.x += amt.as_double();
            m_spacer.w -= amt.as_double();
            break;
        }
        break;
    }
    case opcode::CLEAR:     m_selected.clear(); break;
    case opcode::SETVAR:    m_selected.set_value(m_vars.pop(), cmd.get_args<opcode::SETVAR>()); break;
    case opcode::PUSHVIEW:  m_vars.push(m_contents.top().view()); break;
    case opcode::PUSHNUM:   m_vars.push(cmd.get_args<opcode::PUSHNUM>()); break;
    case opcode::PUSHSTR:   m_vars.push(cmd.get_args<opcode::PUSHSTR>()); break;
    case opcode::PUSHNULL:  m_vars.push(variable::null_var()); break;
    case opcode::PUSHVAR:   m_vars.push(m_selected.get_value()); break;
    case opcode::MOVEVAR:   m_vars.push(m_selected.get_moved()); break;
    case opcode::JMP:
        m_program_counter += cmd.get_args<opcode::JMP>();
        m_jumped = true;
        break;
    case opcode::JSR:
        jump_subroutine(m_program_counter + cmd.get_args<opcode::JSR>());
        break;
    case opcode::JZ:
        if (!m_vars.pop().as_bool()) {
            m_program_counter += cmd.get_args<opcode::JZ>();
            m_jumped = true;
        }
        break;
    case opcode::JNZ:
        if (m_vars.pop().as_bool()) {
            m_program_counter += cmd.get_args<opcode::JNZ>();
            m_jumped = true;
        }
        break;
    case opcode::JTE:
        if (m_contents.top().token_end()) {
            m_program_counter += cmd.get_args<opcode::JTE>();
            m_jumped = true;
        }
        break;
    case opcode::MOVCONTENT: m_contents.push(std::move(m_vars.pop().str())); break;
    case opcode::POPCONTENT: m_contents.pop(); break;
    case opcode::SETBEGIN:  m_contents.top().setbegin(m_vars.pop().as_int()); break;
    case opcode::SETEND:    m_contents.top().setend(m_vars.pop().as_int()); break;
    case opcode::NEWVIEW: m_contents.top().new_view(); break;
    case opcode::SUBVIEW: m_contents.top().new_subview(); break;
    case opcode::RESETVIEW: m_contents.top().reset_view(); break;
    case opcode::NEXTRESULT: m_contents.top().next_result(); break;
    case opcode::NEXTTABLE: ++m_table_index; break;
    case opcode::ATE: m_vars.push(m_last_box_page > m_doc->num_pages()); break;
    case opcode::RET:
        if (!m_return_addrs.empty()) {
            m_program_counter = m_return_addrs.pop();
            break;
        }
        [[fallthrough]];
    case opcode::HLT: halt(); break;
    case opcode::SETLAYOUT:
        if (m_flags & READER_HALT_ON_SETLAYOUT) {
            m_layouts.push_back(cmd.get_args<opcode::SETLAYOUT>());
            halt();
            break;
        }
        [[fallthrough]];
    case opcode::IMPORT: {
        size_t addr = add_layout(bill_layout_script::from_file(cmd.get_args<opcode::IMPORT>()));
        m_code[m_program_counter] = command_args{opcode::JSR, jump_address(addr - m_program_counter)};
        jump_subroutine(addr);
        break;
    }
    case opcode::SETLANG: intl::set_language(cmd.get_args<opcode::SETLANG>()); break;
    }
}

size_t reader::add_layout(const bill_layout_script &layout) {
    size_t new_addr = m_code.size();

    m_layouts.push_back(std::filesystem::canonical(layout.m_filename).string());
    
    auto new_code = parser(layout).get_bytecode();
    std::copy(
        std::move_iterator(new_code.begin()),
        std::move_iterator(new_code.end()),
        std::back_inserter(m_code)
    );
    return new_addr;
}