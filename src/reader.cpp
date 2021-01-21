#include "reader.h"

#include "utils.h"
#include "parser.h"

void reader::start() {
    m_vars.clear();
    m_contents.clear();
    m_refs.clear();
    m_return_addrs.clear();

    m_spacer = {};
    m_last_box_page = 0;

    m_current_table = 0;
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
}

void reader::exec_command(const command_args &cmd) {
    auto exec_operator = [&](auto op) {
        auto ret = op(*(m_vars.rbegin() + 1), m_vars.top());
        m_vars.resize(m_vars.size() - 2);
        m_vars.push(std::move(ret));
    };

    auto create_ref = [&](const std::string &name, auto ... args) {
        if (name.front() == '*') {
            return variable_ref(m_out.globals, name.substr(1), args ...);
        } else {
            while (m_out.values.size() <= m_current_table) m_out.values.emplace_back();
            return variable_ref(m_out.values[m_current_table], name, args ...);
        }
    };

    switch(cmd.command()) {
    case opcode::NOP:
    case opcode::COMMENT:
        break;
    case opcode::RDBOX:
    case opcode::RDPAGE:
    case opcode::RDFILE:
        read_box(cmd.get<pdf_rect>());
        break;
    case opcode::SETPAGE:
        set_page(cmd.get<small_int>());
        break;
    case opcode::CALL:
    {
        const auto &call = cmd.get<command_call>();
        call_function(call.name, call.numargs);
        break;
    }
    case opcode::THROWERR: throw layout_error(m_vars.top().str()); break;
    case opcode::ADDWARNING:
        m_out.warnings.push_back(std::move(m_vars.top().str()));
        m_vars.pop();
        break;
    case opcode::PARSENUM:
        if (m_vars.top().type() == VAR_STRING) {
            m_vars.top() = variable::str_to_number(parse_number(m_vars.top().str()));
        }
        break;
    case opcode::PARSEINT: m_vars.top() = m_vars.top().as_int(); break;
    case opcode::NOT: m_vars.top() = !m_vars.top(); break;
    case opcode::NEG: m_vars.top() = -m_vars.top(); break;
    case opcode::EQ:  exec_operator([](const auto &a, const auto &b) { return a == b; }); break;
    case opcode::NEQ: exec_operator([](const auto &a, const auto &b) { return a != b; }); break;
    case opcode::AND: exec_operator([](const auto &a, const auto &b) { return a && b; }); break;
    case opcode::OR:  exec_operator([](const auto &a, const auto &b) { return a || b; }); break;
    case opcode::ADD: exec_operator([](const auto &a, const auto &b) { return a + b; }); break;
    case opcode::SUB: exec_operator([](const auto &a, const auto &b) { return a - b; }); break;
    case opcode::MUL: exec_operator([](const auto &a, const auto &b) { return a * b; }); break;
    case opcode::DIV: exec_operator([](const auto &a, const auto &b) { return a / b; }); break;
    case opcode::GT:  exec_operator([](const auto &a, const auto &b) { return a > b; }); break;
    case opcode::LT:  exec_operator([](const auto &a, const auto &b) { return a < b; }); break;
    case opcode::GEQ: exec_operator([](const auto &a, const auto &b) { return a >= b; }); break;
    case opcode::LEQ: exec_operator([](const auto &a, const auto &b) { return a <= b; }); break;
    case opcode::SELVARTOP:
    {
        m_refs.emplace_back(create_ref(cmd.get<std::string>(),
            m_vars.top().as_int(), 1));
        m_vars.pop();
        break;
    }
    case opcode::SELRANGEALL:
    {
        auto ref = create_ref(cmd.get<std::string>());
        ref.range_len = ref.size();
        m_refs.push(std::move(ref));
        break;
    }
    case opcode::SELVAR:
    case opcode::SELRANGE:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        auto ref = create_ref(var_idx.name,
            var_idx.index, var_idx.range_len);
        if (var_idx.index == (small_int) -1) {
            ref.index = ref.size();
        }
        m_refs.push(std::move(ref));
        break;
    }
    case opcode::SELRANGETOP:
    {
        auto ref = create_ref(cmd.get<std::string>());
        ref.index = m_vars.top().as_int();
        if (ref.index == (small_int) -1) {
            ref.index = ref.size();
        }
        m_vars.pop();
        ref.range_len = m_vars.top().as_int();
        m_vars.pop();
        m_refs.push(std::move(ref));
        break;
    }
    case opcode::MVBOX:
    {
        switch (cmd.get<spacer_index>()) {
        case spacer_index::SPACER_PAGE:
            m_spacer.page += m_vars.top().as_int();
            break;
        case spacer_index::SPACER_X:
            m_spacer.x += m_vars.top().as_double();
            break;
        case spacer_index::SPACER_Y:
            m_spacer.y += m_vars.top().as_double();
            break;
        case spacer_index::SPACER_W:
            m_spacer.w += m_vars.top().as_double();
            break;
        case spacer_index::SPACER_H:
            m_spacer.h += m_vars.top().as_double();
            break;
        }
        m_vars.pop();
        break;
    }
    case opcode::CLEAR:
        m_refs.top().clear();
        m_refs.pop();
        break;
    case opcode::SETVAR:
        m_refs.top().set_value(std::move(m_vars.top()), SET_ASSIGN);
        m_vars.pop();
        m_refs.pop();
        break;
    case opcode::RESETVAR:
        m_refs.top().set_value(std::move(m_vars.top()), SET_RESET);
        m_vars.pop();
        m_refs.pop();
        break;
    case opcode::PUSHVIEW: m_vars.push(m_contents.top().view()); break;
    case opcode::PUSHBYTE:  m_vars.push(cmd.get<int8_t>()); break;
    case opcode::PUSHSHORT: m_vars.push(cmd.get<int16_t>()); break;
    case opcode::PUSHINT:   m_vars.push(cmd.get<int32_t>()); break;
    case opcode::PUSHDECIMAL: m_vars.push(cmd.get<fixed_point>()); break;
    case opcode::PUSHSTR:   m_vars.push(cmd.get<std::string>()); break;
    case opcode::PUSHVAR:
        m_vars.push(m_refs.top().get_value());
        m_refs.pop();
        break;
    case opcode::MOVEVAR:
        m_vars.push(m_refs.top().get_moved());
        m_refs.pop();
        break;
    case opcode::JMP:
        m_program_counter += cmd.get<jump_address>();
        m_jumped = true;
        break;
    case opcode::JSR:
        m_return_addrs.push(m_program_counter);
        m_program_counter += cmd.get<jump_address>();
        m_jumped = true;
        break;
    case opcode::JZ:
        if (!m_vars.top().as_bool()) {
            m_program_counter += cmd.get<jump_address>();
            m_jumped = true;
        }
        m_vars.pop();
        break;
    case opcode::JNZ:
        if (m_vars.top().as_bool()) {
            m_program_counter += cmd.get<jump_address>();
            m_jumped = true;
        }
        m_vars.pop();
        break;
    case opcode::JTE:
        if (m_contents.top().token_end()) {
            m_program_counter += cmd.get<jump_address>();
            m_jumped = true;
        }
        break;
    case opcode::INC:
        m_refs.top().set_value(std::move(m_vars.top()), SET_INCREASE);
        m_vars.pop();
        m_refs.pop();
        break;
    case opcode::DEC:
        m_refs.top().set_value(- m_vars.top(), SET_INCREASE);
        m_vars.pop();
        m_refs.pop();
        break;
    case opcode::ISSET:
        m_vars.push(m_refs.top().size() != 0);
        m_refs.pop();
        break;
    case opcode::GETSIZE:
        m_vars.push(m_refs.top().size());
        m_refs.pop();
        break;
    case opcode::MOVCONTENT:
        m_contents.push(std::move(m_vars.top().str()));
        m_vars.pop();
        break;
    case opcode::POPCONTENT: m_contents.pop(); break;
    case opcode::SETBEGIN:
        m_contents.top().setbegin(m_vars.top().as_int());
        m_vars.pop();
        break;
    case opcode::SETEND:
        m_contents.top().setend(m_vars.top().as_int());
        m_vars.pop();
        break;
    case opcode::NEWVIEW: m_contents.top().new_view(); break;
    case opcode::NEWTOKENS: m_contents.top().new_tokens(); break;
    case opcode::RESETVIEW: m_contents.top().reset_view(); break;
    case opcode::NEXTLINE: m_contents.top().next_token("\n"); break;
    case opcode::NEXTTOKEN: m_contents.top().next_token("\t\n\v\f\r "); break;
    case opcode::NEXTTABLE: ++m_current_table; break;
    case opcode::ATE: m_vars.push(m_last_box_page > m_doc->num_pages()); break;
    case opcode::RET:
        if (m_return_addrs.empty()) {
            halt();
        } else {
            m_program_counter = m_return_addrs.top();
            m_return_addrs.pop();
        }
        break;
    case opcode::IMPORT:
        import_layout(cmd.get<std::string>());
        break;
    }
}

void reader::import_layout(const std::string &layout_name) {
    m_out.layout_name = layout_name;

    auto new_addr = m_code.size();
    auto new_code = bytecode(m_code.layout_dir / (layout_name + ".bls"));
    std::copy(
        std::move_iterator(new_code.begin()),
        std::move_iterator(new_code.end()),
        std::back_inserter(m_code)
    );
    m_return_addrs.push(m_program_counter);
    m_program_counter = new_addr;
    m_jumped = true;
}

void reader::set_page(int page) {
    page += m_spacer.page;
    m_spacer = {};
    m_last_box_page = page;
    m_contents = {};
}

void reader::read_box(pdf_rect box) {
    box.page += m_spacer.page;
    box.x += m_spacer.x;
    box.y += m_spacer.y;
    box.w += m_spacer.w;
    box.h += m_spacer.h;
    m_spacer = {};

    m_last_box_page = box.page;

    m_contents = {};

    m_contents.push(m_doc->get_text(box));
}