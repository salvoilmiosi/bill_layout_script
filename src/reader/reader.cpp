#include "reader.h"

#include <fmt/core.h>
#include "utils.h"

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
    case NOP:
    case STRDATA:
        break;
    case RDBOX:
    case RDPAGE:
    case RDFILE:
        read_box(cmd.get<pdf_rect>());
        break;
    case CALL:
    {
        const auto &call = cmd.get<command_call>();
        call_function(m_code.get_string(call.name), call.numargs);
        break;
    }
    case ERROR: throw layout_error(m_var_stack.top().str()); break;
    case PARSENUM: if (!m_var_stack.top().empty()) m_var_stack.top() = m_var_stack.top().str_to_number(); break;
    case PARSEINT: m_var_stack.top() = m_var_stack.top().as_int(); break;
    case NOT: m_var_stack.top() = !m_var_stack.top(); break;
    case NEG: m_var_stack.top() = -m_var_stack.top(); break;
    case EQ:  exec_operator([](const auto &a, const auto &b) { return a == b; }); break;
    case NEQ: exec_operator([](const auto &a, const auto &b) { return a != b; }); break;
    case AND: exec_operator([](const auto &a, const auto &b) { return a && b; }); break;
    case OR:  exec_operator([](const auto &a, const auto &b) { return a || b; }); break;
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
        ref.index_first = m_var_stack.top().as_int();
        ref.index_last = ref.index_first;
        m_var_stack.pop();
        m_ref_stack.emplace(std::move(ref));
        break;
    }
    case SELVARALL:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.flags |= VAR_RANGE_ALL;
        m_ref_stack.emplace(std::move(ref));
        break;
    }
    case SELVARIDX:
    case SELVARRANGE:
    {
        variable_ref ref;
        const auto &var_idx = cmd.get<variable_idx>();
        ref.name = m_code.get_string(var_idx.name);
        ref.index_first = var_idx.index_first;
        ref.index_last = var_idx.index_last;
        m_ref_stack.emplace(std::move(ref));
        break;
    }
    case SELVARRANGETOP:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.index_last = m_var_stack.top().as_int();
        m_var_stack.pop();
        ref.index_first = m_var_stack.top().as_int();
        m_var_stack.pop();
        m_ref_stack.emplace(std::move(ref));
        break;
    }
    case SELGLOBAL:
    {
        variable_ref ref;
        ref.name = get_string_ref();
        ref.flags |= VAR_GLOBAL;
        m_ref_stack.emplace(std::move(ref));
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
    case SETDEBUG: m_ref_stack.top().flags |= VAR_DEBUG; break;
    case CLEAR: clear_ref(); break;
    case APPEND:
        m_ref_stack.top().index_first = get_ref_size();
        m_ref_stack.top().index_last = m_ref_stack.top().index_first;
        // fall through
    case SETVAR:
        set_ref();
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case RESETVAR:
        reset_ref();
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case COPYCONTENT: m_var_stack.push(m_content_stack.top().view()); break;
    case PUSHINT: m_var_stack.push(cmd.get<small_int>()); break;
    case PUSHFLOAT: m_var_stack.push(cmd.get<float>()); break;
    case PUSHSTR: m_var_stack.push(get_string_ref()); break;
    case PUSHVAR:
        m_var_stack.push(get_ref());
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
        inc_ref(m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case INC:
        inc_ref(cmd.get<small_int>());
        m_ref_stack.pop();
        break;
    case DECTOP:
        inc_ref(- m_var_stack.top());
        m_var_stack.pop();
        m_ref_stack.pop();
        break;
    case DEC:
        inc_ref(- cmd.get<small_int>());
        m_ref_stack.pop();
        break;
    case ISSET:
        m_var_stack.push(get_ref_size() != 0);
        m_ref_stack.pop();
        break;
    case SIZE:
        m_var_stack.push((int) get_ref_size());
        m_ref_stack.pop();
        break;
    case PUSHCONTENT:
        if (m_var_stack.top().type() == VAR_STRING) {
            m_content_stack.emplace(std::move(m_var_stack.top().strref()));
        } else {
            m_content_stack.push(m_var_stack.top().str());
        }
        m_var_stack.pop();
        break;
    case POPCONTENT: m_content_stack.pop(); break;
    case NEXTLINE: m_content_stack.top().next_token("\n"); break;
    case NEXTTOKEN: m_content_stack.top().next_token("\t\n\v\f\r "); break;
    case NEXTPAGE: ++m_page_num; break;
    case ATE: m_var_stack.push(m_ate); break;
    case HLT: m_programcounter = m_code.m_commands.size(); break;
    }
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
    if (m_ref_stack.top().flags & VAR_GLOBAL) {
        return get_global(m_ref_stack.top().name);
    } else {
        return get_variable(m_ref_stack.top().name, m_ref_stack.top().index_first, m_page_num);
    }
}

void reader::set_ref() {
    auto &value = m_var_stack.top();
    if (value.empty()) return;
    auto &ref = m_ref_stack.top();
    if (ref.flags & VAR_GLOBAL) {
        auto &var = m_globals[ref.name] = std::move(value);
        if (ref.flags & VAR_DEBUG) var.m_debug = true;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[ref.name];
    if (ref.flags & VAR_DEBUG) var.m_debug = true;
    if (ref.flags & VAR_RANGE_ALL) {
        for (auto &x : var) { 
            x = std::move(value);
        }
    } else {
        while (var.size() <= ref.index_last) var.emplace_back();
        for (size_t i=ref.index_first; i<=ref.index_last; ++i) {
            var[i] = std::move(value);
        }
    }
}

void reader::inc_ref(const variable &value) {
    if (value.empty()) return;
    auto &ref = m_ref_stack.top();
    if (ref.flags & VAR_GLOBAL) {
        auto &var = m_globals[ref.name];
        var = var + value;
        if (ref.flags & VAR_DEBUG) var.m_debug = true;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[ref.name];
    if (ref.flags & VAR_DEBUG) var.m_debug = true;
    if (ref.flags & VAR_RANGE_ALL) {
        for (auto &x : var) { 
            x += value;
        }
    } else {
        while (var.size() <= ref.index_last) var.emplace_back();
        for (size_t i=ref.index_first; i<=ref.index_last; ++i) {
            var[i] += value;
        }
    }
}

void reader::reset_ref() {
    auto &value = m_var_stack.top();
    if (value.empty()) return;
    auto &ref = m_ref_stack.top();
    if (ref.flags & VAR_GLOBAL) {
        auto &var = m_globals[ref.name] = std::move(value);
        if (ref.flags & VAR_DEBUG) var.m_debug = true;
        return;
    }

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[ref.name];
    if (ref.flags & VAR_DEBUG) var.m_debug = true;
    var.clear();
    var.emplace_back(std::move(value));
}

void reader::clear_ref() {
    auto &ref = m_ref_stack.top();
    if (ref.flags & VAR_GLOBAL) {
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

size_t reader::get_ref_size() {
    auto &ref = m_ref_stack.top();
    if (ref.flags & VAR_GLOBAL) {
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
            if (pair.second.m_debug) {
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
        std::string name = pair.first;
        if (pair.second.m_debug) {
            if (debug) {
                name = "!" + name;
            } else {
                continue;
            }
        }
        globals[name] = pair.second.str();
    }
}