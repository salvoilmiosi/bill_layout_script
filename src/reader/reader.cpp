#include "reader.h"

#include <fmt/core.h>
#include "utils.h"
#include "functions.h"

void reader::exec_program(std::istream &input) {
    if (m_code.read_bytecode(input).fail()) {
        throw layout_error("File layout non riconosciuto");
    }

    m_var_stack = {};
    m_content_stack = {};
    m_ref_stack = {};
    m_spacer = {};
    m_programcounter = 0;
    m_jumped = false;
    m_global_flag = false;
    m_box_page = 0;

    while (m_programcounter < m_code.m_commands.size()) {
        exec_command(m_code.m_commands[m_programcounter]);
        if (!m_jumped) {
            ++m_programcounter;
        } else {
            m_jumped = false;
        }
    }
}

void reader::exec_command(const command_args &cmd) {
    auto exec_operator = [&](auto op) {
        auto ret = op(*(m_var_stack.rbegin() + 1), m_var_stack.top());
        m_var_stack.resize(m_var_stack.size() - 2);
        m_var_stack.push(std::move(ret));
    };

    auto read_str_ref = [&]() {
        return m_code.get_string(cmd.get<string_ref>());
    };

    switch(cmd.command) {
    case opcode::NOP:
    case opcode::DBGDATA:
    case opcode::STRDATA:
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
        call_function(m_code.get_string(call.name), call.numargs);
        break;
    }
    case opcode::THROWERR: throw layout_error(m_var_stack.top().str()); break;
    case opcode::PARSENUM:
        if (m_var_stack.top().type() == VAR_STRING) {
            m_var_stack.top() = variable::str_to_number(parse_number(m_var_stack.top().str()));
        }
        break;
    case opcode::PARSEINT: m_var_stack.top() = m_var_stack.top().as_int(); break;
    case opcode::NOT: m_var_stack.top() = !m_var_stack.top(); break;
    case opcode::NEG: m_var_stack.top() = -m_var_stack.top(); break;
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
    case opcode::MAX: exec_operator([](const auto &a, const auto &b) { return a > b ? a : b; }); break;
    case opcode::MIN: exec_operator([](const auto &a, const auto &b) { return a < b ? a : b; }); break;
    case opcode::SELVARTOP:
    {
        variable_ref ref(current_page(), read_str_ref(),
            m_var_stack.top().as_int(), 1);
        m_var_stack.pop();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELRANGEALL:
    {
        variable_ref ref(current_page(), read_str_ref());
        ref.range_len = ref.size();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELVAR:
    case opcode::SELRANGE:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        variable_ref ref(current_page(), m_code.get_string(var_idx.name),
            var_idx.index, var_idx.range_len);
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELRANGETOP:
    {
        variable_ref ref(current_page(), read_str_ref());
        ref.index = m_var_stack.top().as_int();
        m_var_stack.pop();
        ref.range_len = m_var_stack.top().as_int();
        m_var_stack.pop();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELGLOBAL:
        m_global_flag = true;
        break;
    case opcode::MVBOX:
    {
        switch (cmd.get<spacer_index>()) {
        case spacer_index::SPACER_PAGE:
            m_spacer.page += m_var_stack.top().as_int();
            break;
        case spacer_index::SPACER_X:
            m_spacer.x += m_var_stack.top().as_double();
            break;
        case spacer_index::SPACER_Y:
            m_spacer.y += m_var_stack.top().as_double();
            break;
        case spacer_index::SPACER_W:
            m_spacer.w += m_var_stack.top().as_double();
            break;
        case spacer_index::SPACER_H:
            m_spacer.h += m_var_stack.top().as_double();
            break;
        }
        m_var_stack.pop();
        break;
    }
    case opcode::CLEAR:
        m_ref_stack.top().clear();
        m_ref_stack.pop();
        break;
    case opcode::APPEND:
        m_ref_stack.top().index = m_ref_stack.top().size();
        break;
    case opcode::SETVAR:
        m_ref_stack.top().set_value(std::move(m_var_stack.top()), SET_ASSIGN);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::RESETVAR:
        m_ref_stack.top().set_value(std::move(m_var_stack.top()), SET_RESET);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::PUSHVIEW: m_var_stack.push(m_content_stack.top().view()); break;
    case opcode::PUSHBYTE:  m_var_stack.push(cmd.get<int8_t>()); break;
    case opcode::PUSHSHORT: m_var_stack.push(cmd.get<int16_t>()); break;
    case opcode::PUSHINT:   m_var_stack.push(cmd.get<int32_t>()); break;
    case opcode::PUSHDECIMAL: m_var_stack.push(cmd.get<fixed_point>()); break;
    case opcode::PUSHSTR:   m_var_stack.push(read_str_ref()); break;
    case opcode::PUSHVAR:
        m_var_stack.push(m_ref_stack.top().get_value());
        m_ref_stack.pop();
        break;
    case opcode::MOVEVAR:
    {
        variable buf;
        if (m_ref_stack.top().move_into(buf)) {
            m_ref_stack.top().clear();
        }
        m_var_stack.push(std::move(buf));
        m_ref_stack.pop();
        break;
    }
    case opcode::JMP:
        m_programcounter = cmd.get<jump_address>();
        m_jumped = true;
        break;
    case opcode::JZ:
        if (!m_var_stack.top().as_bool()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        m_var_stack.pop();
        break;
    case opcode::JNZ:
        if (m_var_stack.top().as_bool()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        m_var_stack.pop();
        break;
    case opcode::JTE:
        if (m_content_stack.top().token_end()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        break;
    case opcode::INC:
        m_ref_stack.top().set_value(std::move(m_var_stack.top()), SET_INCREASE);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::DEC:
        m_ref_stack.top().set_value(- m_var_stack.top(), SET_INCREASE);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::ISSET:
        m_var_stack.push(m_ref_stack.top().size() != 0);
        m_ref_stack.pop();
        break;
    case opcode::GETSIZE:
        m_var_stack.push(m_ref_stack.top().size());
        m_ref_stack.pop();
        break;
    case opcode::MOVCONTENT:
        m_content_stack.push(std::move(m_var_stack.top().str()));
        m_var_stack.pop();
        break;
    case opcode::POPCONTENT: m_content_stack.pop(); break;
    case opcode::SETBEGIN:
        m_content_stack.top().setbegin(m_var_stack.top().as_int());
        m_var_stack.pop();
        break;
    case opcode::SETEND:
        m_content_stack.top().setend(m_var_stack.top().as_int());
        m_var_stack.pop();
        break;
    case opcode::NEXTLINE: m_content_stack.top().next_token("\n"); break;
    case opcode::NEXTTOKEN: m_content_stack.top().next_token("\t\n\v\f\r "); break;
    case opcode::NEXTPAGE: ++m_page_num; break;
    case opcode::ATE: m_var_stack.push(m_box_page > m_doc.num_pages()); break;
    case opcode::HLT: m_programcounter = m_code.m_commands.size(); break;
    }
}

void reader::set_page(int page) {
    page += m_spacer.page;
    m_spacer = {};
    m_box_page = page;
    m_content_stack = {};
}

void reader::read_box(pdf_rect box) {
    box.page += m_spacer.page;
    box.x += m_spacer.x;
    box.y += m_spacer.y;
    box.w += m_spacer.w;
    box.h += m_spacer.h;
    m_spacer = {};

    m_box_page = box.page;

    m_content_stack = {};

    try {
        m_content_stack.push(m_doc.get_text(box));
    } catch (const pdf_error &error) {
        throw layout_error(error.message);
    }
}

variable_page &reader::current_page() {
    if (m_global_flag) {
        m_global_flag = false;
        return m_globals;
    } else {
        while (m_pages.size() <= m_page_num) m_pages.emplace_back();
        return m_pages[m_page_num];
    }
}

void reader::save_output(Json::Value &root, bool debug) {
    auto write_page = [&](const variable_page &page) {
        Json::Value ret = Json::objectValue;
        for (auto &pair : page) {
            const std::string &name = pair.first;
            if (name.front() == '_' && !debug) {
                continue;
            }
            auto &json_arr = ret[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(pair.second.str());
        }
        return ret;
    };

    root["globals"] = write_page(m_globals);
    
    Json::Value &values = root["values"] = Json::arrayValue;
    for (auto &page : m_pages) {
        values.append(write_page(page));
    }
}