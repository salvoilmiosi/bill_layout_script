#include "reader.h"

#include <json/json.h>
#include <fmt/core.h>
#include "../shared/utils.h"

void reader::read_layout(const pdf_info &info, std::istream &input) {
    m_asm.read_bytecode(input);

    m_var_stack = {};
    m_content_stack = {};
    m_ref_stack = {};
    m_spacer = {};
    m_programcounter = 0;
    m_jumped = false;

    while (m_programcounter < m_asm.m_commands.size()) {
        exec_command(info, m_asm.m_commands[m_programcounter]);
        if (!m_jumped) {
            ++m_programcounter;
        } else {
            m_jumped = false;
        }
    }
}

void content_view::next_token(const std::string &separator) {
    if (token_start == (size_t) -1) {
        token_start = token_end = 0;
    }
    token_start = text.find_first_not_of(separator, token_end);
    if (token_start == std::string::npos) {
        token_start = token_end = text.size();
    } else {
        token_end = text.find_first_of(separator, token_start);
    }
}

std::string_view content_view::view() const {
    if (token_start == (size_t) -1) {
        return std::string_view(text);
    } else {
        return std::string_view(text).substr(token_start, token_end - token_start);
    }
}

void reader::exec_command(const pdf_info &info, const command_args &cmd) {
    auto exec_operator = [&](auto op) {
        auto var2 = m_var_stack.top();
        m_var_stack.pop();
        auto var1 = m_var_stack.top();
        m_var_stack.pop();
        auto var = op(std::move(var1), std::move(var2));
        m_var_stack.push(var);
    };

    auto get_string_ref = [&]() {
        return m_asm.get_string(cmd.get<string_ref>());
    };

    switch(cmd.command) {
    case NOP:
    case STRDATA:
        break;
    case RDBOX:
    case RDPAGE:
    case RDFILE:
        read_box(info, cmd.get<pdf_rect>());
        break;
    case CALL:
    {
        const auto &call = cmd.get<command_call>();
        call_function(m_asm.get_string(call.name), call.numargs);
        break;
    }
    case ERROR: throw layout_error(get_string_ref()); break;
    case PARSENUM: if (!m_var_stack.top().empty()) m_var_stack.top() = m_var_stack.top().str_to_number(); break;
    case PARSEINT: m_var_stack.top() = m_var_stack.top().as_int(); break;
    case NOT: m_var_stack.top() = !m_var_stack.top().as_bool(); break;
    case NEG: m_var_stack.top() = -m_var_stack.top(); break;
    case EQ:  exec_operator([](const auto &a, const auto &b) { return a == b; }); break;
    case NEQ: exec_operator([](const auto &a, const auto &b) { return a != b; }); break;
    case AND: exec_operator([](const auto &a, const auto &b) { return a.as_bool() && b.as_bool(); }); break;
    case OR:  exec_operator([](const auto &a, const auto &b) { return a.as_bool() || b.as_bool(); }); break;
    case ADD: exec_operator([](const auto &a, const auto &b) { return a + b; }); break;
    case SUB: exec_operator([](const auto &a, const auto &b) { return a - b; }); break;
    case MUL: exec_operator([](const auto &a, const auto &b) { return a * b; }); break;
    case DIV: exec_operator([](const auto &a, const auto &b) { return a / b; }); break;
    case GT:  exec_operator([](const auto &a, const auto &b) { return a > b; }); break;
    case LT:  exec_operator([](const auto &a, const auto &b) { return a < b; }); break;
    case GEQ: exec_operator([](const auto &a, const auto &b) { return a >= b; }); break;
    case LEQ: exec_operator([](const auto &a, const auto &b) { return a <= b; }); break;
    case MAX: exec_operator([](const auto &a, const auto &b) { return a > b ? a : b; }); break;
    case MIN: exec_operator([](const auto &a, const auto &b) { return a < b ? a : b; }); break;
    case SELVAR:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index = m_var_stack.top().as_int();
        m_var_stack.pop();
        m_ref_stack.push(std::move(ref));
        break;
    }
    case SELVARIDX:
    {
        variable_ref ref;
        const auto &var_idx = cmd.get<variable_idx>();
        ref.name = m_asm.get_string(var_idx.name);
        ref.index = var_idx.index;
        m_ref_stack.push(std::move(ref));
        break;
    }
    case SELGLOBAL:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index = INDEX_GLOBAL;
        m_ref_stack.push(std::move(ref));
        break;
    }
    case MVBOX:
    {
        switch (static_cast<spacer_index>(cmd.get<small_int>())) {
        case SPACER_PAGE:
            m_spacer.page += m_var_stack.top().as_int();
            break;
        case SPACER_X:
            m_spacer.x += m_var_stack.top().number().getAsDouble();
            break;
        case SPACER_Y:
            m_spacer.y += m_var_stack.top().number().getAsDouble();
            break;
        case SPACER_W:
            m_spacer.w += m_var_stack.top().number().getAsDouble();
            break;
        case SPACER_H:
            m_spacer.h += m_var_stack.top().number().getAsDouble();
            break;
        }
        m_var_stack.pop();
        break;
    }
    case SETDEBUG: m_var_stack.top().set_debug(true); break;
    case CLEAR: clear_variable(); break;
    case APPEND:
        m_ref_stack.top().index = get_variable_size();
        // fall through
    case SETVAR:
        set_variable(m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case RESETVAR:
        reset_variable(m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case COPYCONTENT: m_var_stack.push(m_content_stack.top().view()); break;
    case PUSHINT: m_var_stack.push(cmd.get<small_int>()); break;
    case PUSHFLOAT: m_var_stack.push(cmd.get<float>()); break;
    case PUSHSTR: m_var_stack.push(get_string_ref()); break;
    case PUSHVAR:
        m_var_stack.push(get_variable());
        m_ref_stack.pop();
        break;
    case JMP:
        m_programcounter = cmd.get<jump_address>();
        m_jumped = true;
        break;
    case JZ:
        if (!m_var_stack.top().as_bool()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        m_var_stack.pop();
        break;
    case JNZ:
        if (m_var_stack.top().as_bool()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        m_var_stack.pop();
        break;
    case JTE:
        if (m_content_stack.top().token_start == m_content_stack.top().text.size()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        break;
    case INCTOP:
        if (!m_var_stack.top().empty()) {
            set_variable(get_variable() + m_var_stack.top());
        }
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case INC:
        set_variable(get_variable() + variable(cmd.get<small_int>()));
        m_ref_stack.pop();
        break;
    case DECTOP:
        if (!m_var_stack.top().empty()) {
            set_variable(get_variable() - m_var_stack.top());
        }
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case DEC:
        set_variable(get_variable() - variable(cmd.get<small_int>()));
        m_ref_stack.pop();
        break;
    case ISSET:
        m_var_stack.push(get_variable_size() != 0);
        m_ref_stack.pop();
        break;
    case SIZE:
        m_var_stack.push((int) get_variable_size());
        m_ref_stack.pop();
        break;
    case PUSHCONTENT:
        m_content_stack.push(m_var_stack.top().str());
        m_var_stack.pop();
        break;
    case POPCONTENT: m_content_stack.pop(); break;
    case NEXTLINE: m_content_stack.top().next_token("\n"); break;
    case NEXTTOKEN: m_content_stack.top().next_token("\t\n\v\f\r "); break;
    case NEXTPAGE:
        ++m_page_num;
        break;
    case HLT:
        m_programcounter = m_asm.m_commands.size();
        break;
    }
}

void reader::read_box(const pdf_info &info, pdf_rect box) {
    box.page += m_spacer.page;
    box.x += m_spacer.x;
    box.y += m_spacer.y;
    box.w += m_spacer.w;
    box.h += m_spacer.h;
    m_spacer = {};
    m_content_stack = {};
    m_content_stack.push(pdf_to_text(info, box));
}

const variable &reader::get_global(const std::string &name) const {
    auto it = m_globals.find(name);
    if (it == m_globals.end()) {
        return variable::null_var();
    } else {
        return it->second;
    }
}

const variable &reader::get_variable() const {
    if (m_ref_stack.top().index == INDEX_GLOBAL) {
        return get_global(m_ref_stack.top().name);
    }
    if (m_pages.size() <= m_page_num) {
        return variable::null_var();
    }

    auto &page = m_pages[m_page_num];
    auto it = page.find(m_ref_stack.top().name);
    if (it == page.end() || it->second.size() <= m_ref_stack.top().index) {
        return variable::null_var();
    } else {
        return it->second[m_ref_stack.top().index];
    }
}

void reader::set_variable(const variable &value) {
    if (value.empty()) return;
    if (m_ref_stack.top().index == INDEX_GLOBAL) {
        m_globals[m_ref_stack.top().name] = value;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[m_ref_stack.top().name];
    while (var.size() <= m_ref_stack.top().index) var.emplace_back();
    var[m_ref_stack.top().index] = value;
}

void reader::reset_variable(const variable &value) {
    if (value.empty()) return;
    if (m_ref_stack.top().index == INDEX_GLOBAL) {
        m_globals[m_ref_stack.top().name] = value;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[m_ref_stack.top().name];
    var.clear();
    var.push_back(value);
}

void reader::clear_variable() {
    if (m_ref_stack.top().index == INDEX_GLOBAL) {
        auto it = m_globals.find(m_ref_stack.top().name);
        if (it != m_globals.end()) {
            m_globals.erase(it);
        }
    } else if (m_page_num < m_pages.size()) {
        auto &page = m_pages[m_page_num];
        auto it = page.find(m_ref_stack.top().name);
        if (it != page.end()) {
            page.erase(it);
        }
    }
}

size_t reader::get_variable_size() {
    if (m_pages.size() <= m_page_num) {
        return 0;
    }
    auto &page = m_pages[m_page_num];
    auto it = page.find(m_ref_stack.top().name);
    if (it == page.end()) {
        return 0;
    } else {
        return it->second.size();
    }
}

std::ostream &reader::print_output(std::ostream &out, bool debug) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::arrayValue;

    for (auto &page : m_pages) {
        auto &page_values = values.append(Json::objectValue);
        for (auto &pair : page) {
            std::string name = pair.first;
            if (pair.second.front().is_debug()) {
                if (debug) {
                    name = "!" + name;
                } else {
                    continue;
                }
            }
            auto &json_arr = page_values[name] = Json::arrayValue;
            for (auto &val : pair.second) {
                json_arr.append(val.str());
            }
        }
    }

    Json::Value &globals = root["globals"] = Json::objectValue;
    for (auto &pair : m_globals) {
        if (!debug && pair.second.is_debug()) continue;
        globals[pair.first] = pair.second.str();
    }

    return out << root;
}