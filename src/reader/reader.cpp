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
    m_ate = false;

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
        auto var2 = std::move(m_var_stack.top());
        m_var_stack.pop();
        auto var1 = std::move(m_var_stack.top());
        m_var_stack.pop();
        m_var_stack.push(op(var1, var2));
    };

    auto get_string_ref = [&]() {
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
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index = m_var_stack.top().as_int();
        ref.range_len = 1;
        m_var_stack.pop();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELRANGEALL:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index = 0;
        ref.range_len = get_ref_size(ref);
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELVAR:
    case opcode::SELRANGE:
    {
        variable_ref ref;
        const auto &var_idx = cmd.get<variable_idx>();
        ref.name = m_code.get_string(var_idx.name);
        ref.index = var_idx.index;
        ref.range_len = var_idx.range_len;
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELRANGETOP:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index = m_var_stack.top().as_int();
        m_var_stack.pop();
        ref.range_len = m_var_stack.top().as_int();
        m_var_stack.pop();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case opcode::SELGLOBAL:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.isglobal = true;
        m_ref_stack.push(std::move(ref));
        break;
    }
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
    case opcode::CLEAR: clear_ref(); break;
    case opcode::APPEND:
        m_ref_stack.top().index = get_ref_size(m_ref_stack.top());
        // fall through
    case opcode::SETVAR:
        set_ref(false);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::RESETVAR:
        set_ref(true);
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::PUSHVIEW: m_var_stack.push(m_content_stack.top().view()); break;
    case opcode::PUSHBYTE:  m_var_stack.push(cmd.get<int8_t>()); break;
    case opcode::PUSHSHORT: m_var_stack.push(cmd.get<int16_t>()); break;
    case opcode::PUSHINT:   m_var_stack.push(cmd.get<int32_t>()); break;
    case opcode::PUSHDECIMAL: m_var_stack.push(cmd.get<fixed_point>()); break;
    case opcode::PUSHSTR:   m_var_stack.push(get_string_ref()); break;
    case opcode::PUSHVAR:
        m_var_stack.push(get_ref());
        m_ref_stack.pop();
        break;
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
        inc_ref(m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::DEC:
        inc_ref(- m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case opcode::ISSET:
        m_var_stack.push(get_ref_size(m_ref_stack.top()) != 0);
        m_ref_stack.pop();
        break;
    case opcode::GETSIZE:
        m_var_stack.push(get_ref_size(m_ref_stack.top()));
        m_ref_stack.pop();
        break;
    case opcode::PUSHCONTENT:
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
    case opcode::ATE: m_var_stack.push(m_ate); break;
    case opcode::HLT: m_programcounter = m_code.m_commands.size(); break;
    }
}

void reader::set_page(int page) {
    page += m_spacer.page;
    m_spacer = {};
    m_ate = page > m_doc.num_pages();
    m_content_stack = {};
}

void reader::read_box(pdf_rect box) {
    box.page += m_spacer.page;
    box.x += m_spacer.x;
    box.y += m_spacer.y;
    box.w += m_spacer.w;
    box.h += m_spacer.h;
    m_spacer = {};

    m_ate = box.page > m_doc.num_pages();

    m_content_stack = {};

    try {
        m_content_stack.push(m_doc.get_text(box));
    } catch (const pdf_error &error) {
        throw layout_error(error.message);
    }
}

const variable &reader::get_global(const std::string &name) const {
    auto it = m_globals.find(name);
    if (it == m_globals.end()) {
        return variable::null_var();
    } else {
        return it->second;
    }
}

const variable &reader::get_variable(const std::string &name, size_t index, size_t page_idx) const {
    if (m_pages.size() <= m_page_num) {
        return variable::null_var();
    }

    auto &page = m_pages[page_idx];
    auto it = page.find(name);
    if (it == page.end() || it->second.size() <= index) {
        return variable::null_var();
    } else {
        return it->second[index];
    }
}

const variable &reader::get_ref() const {
    if (m_ref_stack.top().isglobal) {
        return get_global(m_ref_stack.top().name);
    } else {
        return get_variable(m_ref_stack.top().name, m_ref_stack.top().index, m_page_num);
    }
}

void reader::set_ref(bool clear) {
    auto &value = m_var_stack.top();
    if (value.empty()) return;
    auto &ref = m_ref_stack.top();
    if (ref.isglobal) {
        m_globals[ref.name] = std::move(value);
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[ref.name];
    if (clear) var.clear();
    while (var.size() < ref.index + ref.range_len) var.emplace_back();
    if (ref.range_len == 1) {
        var[ref.index] = std::move(value);
    } else {
        for (size_t i=ref.index; i < ref.index + ref.range_len; ++i) {
            var[i] = value;
        }
    }
}

void reader::inc_ref(const variable &value) {
    if (value.empty()) return;
    auto &ref = m_ref_stack.top();
    if (ref.isglobal) {
        m_globals[ref.name] += value;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[ref.name];
    while (var.size() < ref.index + ref.range_len) var.emplace_back();
    for (size_t i=ref.index; i < ref.index + ref.range_len; ++i) {
        var[i] += value;
    }
}

void reader::clear_ref() {
    auto &ref = m_ref_stack.top();
    if (ref.isglobal) {
        auto it = m_globals.find(ref.name);
        if (it != m_globals.end()) {
            m_globals.erase(it);
        }
    } else if (m_page_num < m_pages.size()) {
        auto &page = m_pages[m_page_num];
        auto it = page.find(ref.name);
        if (it != page.end()) {
            page.erase(it);
        }
    }
}

size_t reader::get_ref_size(variable_ref &ref) {
    if (ref.isglobal) {
        return m_globals.find(ref.name) != m_globals.end();
    }
    if (m_pages.size() <= m_page_num) {
        return 0;
    }
    auto &page = m_pages[m_page_num];
    auto it = page.find(ref.name);
    if (it == page.end()) {
        return 0;
    } else {
        return it->second.size();
    }
}

void reader::save_output(Json::Value &root, bool debug) {
    Json::Value &values = root["values"] = Json::arrayValue;

    for (auto &page : m_pages) {
        auto &page_values = values.append(Json::objectValue);
        for (auto &pair : page) {
            std::string name = pair.first;
            if (name.front() == '_' && !debug) {
                continue;
            }
            auto &json_arr = page_values[name] = Json::arrayValue;
            for (auto &val : pair.second) {
                json_arr.append(val.str());
            }
        }
    }

    Json::Value &globals = root["globals"] = Json::objectValue;
    for (auto &pair : m_globals) {
        std::string name = pair.first;
        if (name.front() == '_' && !debug) {
            continue;
        }
        globals[name] = pair.second.str();
    }
}