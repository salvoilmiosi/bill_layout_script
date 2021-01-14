#include "reader.h"

#include <fmt/core.h>
#include "utils.h"
#include "functions.h"

void reader::exec_program(std::istream &input) {
    if (m_code.read_bytecode(input).fail()) {
        throw layout_error("File layout non riconosciuto");
    }

    m_con = {};

    while (m_con.program_counter < m_code.m_commands.size()) {
        exec_command(m_code.m_commands[m_con.program_counter]);
        if (!m_con.jumped) {
            ++m_con.program_counter;
        } else {
            m_con.jumped = false;
        }
    }
}

void reader::exec_command(const command_args &cmd) {
    auto exec_operator = [&](auto op) {
        auto ret = op(*(m_con.vars.rbegin() + 1), m_con.vars.top());
        m_con.vars.resize(m_con.vars.size() - 2);
        m_con.vars.push(std::move(ret));
    };

    auto read_str_ref = [&]() {
        return m_code.get_string(cmd.get<string_ref>());
    };

    auto create_ref = [&](const std::string &name, auto ... args) {
        if (name.front() == '*') {
            return variable_ref(m_globals, name.substr(1), args ...);
        } else {
            while (m_values.size() <= m_con.current_vmap) m_values.emplace_back();
            return variable_ref(m_values[m_con.current_vmap], name, args ...);
        }
    };

    switch(cmd.command) {
    case opcode::NOP:
    case opcode::COMMENT:
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
    case opcode::THROWERR: throw layout_error(m_con.vars.top().str()); break;
    case opcode::ADDWARNING:
        m_warnings.push_back(std::move(m_con.vars.top().str()));
        m_con.vars.pop();
        break;
    case opcode::PARSENUM:
        if (m_con.vars.top().type() == VAR_STRING) {
            m_con.vars.top() = variable::str_to_number(parse_number(m_con.vars.top().str()));
        }
        break;
    case opcode::PARSEINT: m_con.vars.top() = m_con.vars.top().as_int(); break;
    case opcode::NOT: m_con.vars.top() = !m_con.vars.top(); break;
    case opcode::NEG: m_con.vars.top() = -m_con.vars.top(); break;
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
        m_con.refs.emplace_back(create_ref(read_str_ref(),
            m_con.vars.top().as_int(), 1));
        m_con.vars.pop();
        break;
    }
    case opcode::SELRANGEALL:
    {
        auto ref = create_ref(read_str_ref());
        ref.range_len = ref.size();
        m_con.refs.push(std::move(ref));
        break;
    }
    case opcode::SELVAR:
    case opcode::SELRANGE:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        auto ref = create_ref(m_code.get_string(var_idx.name),
            var_idx.index, var_idx.range_len);
        if (var_idx.index == (small_int) -1) {
            ref.index = ref.size();
        }
        m_con.refs.push(std::move(ref));
        break;
    }
    case opcode::SELRANGETOP:
    {
        auto ref = create_ref(read_str_ref());
        ref.index = m_con.vars.top().as_int();
        m_con.vars.pop();
        ref.range_len = m_con.vars.top().as_int();
        m_con.vars.pop();
        m_con.refs.push(std::move(ref));
        break;
    }
    case opcode::MVBOX:
    {
        switch (cmd.get<spacer_index>()) {
        case spacer_index::SPACER_PAGE:
            m_con.spacer.page += m_con.vars.top().as_int();
            break;
        case spacer_index::SPACER_X:
            m_con.spacer.x += m_con.vars.top().as_double();
            break;
        case spacer_index::SPACER_Y:
            m_con.spacer.y += m_con.vars.top().as_double();
            break;
        case spacer_index::SPACER_W:
            m_con.spacer.w += m_con.vars.top().as_double();
            break;
        case spacer_index::SPACER_H:
            m_con.spacer.h += m_con.vars.top().as_double();
            break;
        }
        m_con.vars.pop();
        break;
    }
    case opcode::CLEAR:
        m_con.refs.top().clear();
        m_con.refs.pop();
        break;
    case opcode::SETVAR:
        m_con.refs.top().set_value(std::move(m_con.vars.top()), SET_ASSIGN);
        m_con.vars.pop();
        m_con.refs.pop();
        break;
    case opcode::RESETVAR:
        m_con.refs.top().set_value(std::move(m_con.vars.top()), SET_RESET);
        m_con.vars.pop();
        m_con.refs.pop();
        break;
    case opcode::PUSHVIEW: m_con.vars.push(m_con.contents.top().view()); break;
    case opcode::PUSHBYTE:  m_con.vars.push(cmd.get<int8_t>()); break;
    case opcode::PUSHSHORT: m_con.vars.push(cmd.get<int16_t>()); break;
    case opcode::PUSHINT:   m_con.vars.push(cmd.get<int32_t>()); break;
    case opcode::PUSHDECIMAL: m_con.vars.push(cmd.get<fixed_point>()); break;
    case opcode::PUSHSTR:   m_con.vars.push(read_str_ref()); break;
    case opcode::PUSHVAR:
        m_con.vars.push(m_con.refs.top().get_value());
        m_con.refs.pop();
        break;
    case opcode::MOVEVAR:
        m_con.vars.push(m_con.refs.top().get_moved());
        m_con.refs.pop();
        break;
    case opcode::JMP:
        m_con.program_counter = cmd.get<jump_address>();
        m_con.jumped = true;
        break;
    case opcode::JZ:
        if (!m_con.vars.top().as_bool()) {
            m_con.program_counter = cmd.get<jump_address>();
            m_con.jumped = true;
        }
        m_con.vars.pop();
        break;
    case opcode::JNZ:
        if (m_con.vars.top().as_bool()) {
            m_con.program_counter = cmd.get<jump_address>();
            m_con.jumped = true;
        }
        m_con.vars.pop();
        break;
    case opcode::JTE:
        if (m_con.contents.top().token_end()) {
            m_con.program_counter = cmd.get<jump_address>();
            m_con.jumped = true;
        }
        break;
    case opcode::INC:
        m_con.refs.top().set_value(std::move(m_con.vars.top()), SET_INCREASE);
        m_con.vars.pop();
        m_con.refs.pop();
        break;
    case opcode::DEC:
        m_con.refs.top().set_value(- m_con.vars.top(), SET_INCREASE);
        m_con.vars.pop();
        m_con.refs.pop();
        break;
    case opcode::ISSET:
        m_con.vars.push(m_con.refs.top().size() != 0);
        m_con.refs.pop();
        break;
    case opcode::GETSIZE:
        m_con.vars.push(m_con.refs.top().size());
        m_con.refs.pop();
        break;
    case opcode::MOVCONTENT:
        m_con.contents.push(std::move(m_con.vars.top().str()));
        m_con.vars.pop();
        break;
    case opcode::POPCONTENT: m_con.contents.pop(); break;
    case opcode::SETBEGIN:
        m_con.contents.top().setbegin(m_con.vars.top().as_int());
        m_con.vars.pop();
        break;
    case opcode::SETEND:
        m_con.contents.top().setend(m_con.vars.top().as_int());
        m_con.vars.pop();
        break;
    case opcode::NEXTLINE: m_con.contents.top().next_token("\n"); break;
    case opcode::NEXTTOKEN: m_con.contents.top().next_token("\t\n\v\f\r "); break;
    case opcode::NEWVALUES: ++m_con.current_vmap; break;
    case opcode::ATE: m_con.vars.push(m_con.last_box_page > m_doc.num_pages()); break;
    case opcode::HLT: m_con.program_counter = m_code.m_commands.size(); break;
    }
}

void reader::set_page(int page) {
    page += m_con.spacer.page;
    m_con.spacer = {};
    m_con.last_box_page = page;
    m_con.contents = {};
}

void reader::read_box(pdf_rect box) {
    box.page += m_con.spacer.page;
    box.x += m_con.spacer.x;
    box.y += m_con.spacer.y;
    box.w += m_con.spacer.w;
    box.h += m_con.spacer.h;
    m_con.spacer = {};

    m_con.last_box_page = box.page;

    m_con.contents = {};

    try {
        m_con.contents.push(m_doc.get_text(box));
    } catch (const pdf_error &error) {
        throw layout_error(error.message);
    }
}

void reader::save_output(Json::Value &root, bool debug) {
    auto write_values = [&](const variable_map &values) {
        Json::Value ret = Json::objectValue;
        for (auto &[name, var] : values) {
            if (name.front() == '_' && !debug) {
                continue;
            }
            auto &json_arr = ret[name];
            if (json_arr.isNull()) json_arr = Json::arrayValue;
            json_arr.append(var.str());
        }
        return ret;
    };

    root["globals"] = write_values(m_globals);
    
    Json::Value &values = root["values"] = Json::arrayValue;
    for (auto &v : m_values) {
        values.append(write_values(v));
    }

    for (auto &v : m_warnings) {
        Json::Value &warnings = root["warnings"];
        if (warnings.isNull()) warnings = Json::arrayValue;
        warnings.append(v);
    }
}