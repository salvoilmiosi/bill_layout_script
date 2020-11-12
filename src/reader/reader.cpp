#include "reader.h"

#include <json/json.h>
#include <fmt/core.h>
#include "../shared/utils.h"

template<typename T> T readData(std::istream &input) {
    T buffer;
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
    return buffer;
}

void reader::read_layout(const pdf_info &info, std::istream &input) {
    m_commands.clear();
    m_strings.clear();
    m_var_stack.clear();
    m_content_stack.clear();
    m_programcounter = 0;
    m_jumped = false;

    while (!input.eof()) {
        asm_command cmd = static_cast<asm_command>(readData<command_int>(input));
        switch(cmd) {
        case RDBOX:
        {
            command_box box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            box.ref_spacers = readData<string_ref>(input);
            box.x = readData<float>(input);
            box.y = readData<float>(input);
            box.w = readData<float>(input);
            box.h = readData<float>(input);
            m_commands.emplace_back(RDBOX, std::move(box));
            break;
        }
        case RDPAGE:
        {
            command_box box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.page = readData<small_int>(input);
            box.ref_spacers = readData<string_ref>(input);
            m_commands.emplace_back(RDPAGE, std::move(box));
            break;
        }
        case RDFILE:
        {
            command_box box;
            box.type = BOX_WHOLE_FILE;
            box.mode = static_cast<read_mode>(readData<small_int>(input));
            box.ref_spacers = 0xffff;
            m_commands.emplace_back(RDFILE, std::move(box));
            break;
        }
        case CALL:
        {
            command_call call;
            call.name = readData<string_ref>(input);
            call.numargs = readData<small_int>(input);
            m_commands.emplace_back(CALL, std::move(call));
            break;
        }
        case SPACER:
        {
            command_spacer spacer;
            spacer.name = readData<string_ref>(input);
            spacer.w = readData<float>(input);
            spacer.h = readData<float>(input);
            m_commands.emplace_back(SPACER, std::move(spacer));
            break;
        }
        case SETVARIDX:
        case PUSHVARIDX:
        case INCTOPIDX:
        case INCIDX:
        case DECTOPIDX:
        case DECIDX:
        {
            variable_idx var_idx;
            var_idx.name = readData<string_ref>(input);
            var_idx.index = readData<small_int>(input);
            m_commands.emplace_back(cmd, std::move(var_idx));
            break;
        }
        case ERROR:
        case SETGLOBAL:
        case CLEAR:
        case APPEND:
        case SETVAR:
        case RESETVAR:
        case PUSHSTR:
        case PUSHGLOBAL:
        case PUSHVAR:
        case ISSET:
        case SIZE:
        case INC:
        case INCTOP:
        case INCG:
        case INCGTOP:
        case DEC:
        case DECTOP:
        case DECG:
        case DECGTOP:
            m_commands.emplace_back(cmd, readData<string_ref>(input));
            break;
        case PUSHNUM:
            m_commands.emplace_back(cmd, readData<float>(input));
            break;
        case JMP:
        case JZ:
        case JTE:
            m_commands.emplace_back(cmd, readData<uint16_t>(input));
            break;
        case STRDATA:
        {
            string_size len = readData<string_size>(input);
            std::string buffer(len, '\0');
            input.read(buffer.data(), len);
            m_strings.push_back(std::move(buffer));
            break;
        }
        default:
            m_commands.emplace_back(cmd);
        }
    }

    while (m_programcounter < m_commands.size()) {
        exec_command(info, m_commands[m_programcounter]);
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
        auto var = op(m_var_stack[m_var_stack.size()-2], m_var_stack.back());
        m_var_stack.pop_back();
        m_var_stack.pop_back();
        m_var_stack.push_back(var);
    };
    
    auto get_string = [&](string_ref ref) -> std::string {
        if (ref == 0xffff) {
            return "";
        } else {
            return m_strings[ref];
        }
    };

    auto get_string_ref = [&]() {
        return get_string(cmd.get<string_ref>());
    };

    switch(cmd.command) {
    case NOP:
    case STRDATA:
        break;
    case RDBOX:
    case RDPAGE:
    case RDFILE:
    {
        auto box = cmd.get<command_box>();
        box.spacers = get_string(box.ref_spacers);
        read_box(info, box);
        break;
    }
    case CALL:
    {
        const auto &call = cmd.get<command_call>();
        call_function(get_string(call.name), call.numargs);
        break;
    }
    case ERROR: throw layout_error(get_string_ref()); break;
    case PARSENUM:
        if (m_var_stack.back().type() != VALUE_NUMBER) {
            m_var_stack.back() = variable(parse_number(m_var_stack.back().str()), VALUE_NUMBER);
        }
        break;
    case PARSEINT: m_var_stack.back() = variable(std::to_string(m_var_stack.back().asInt()), VALUE_NUMBER); break;
    case EQ:  exec_operator([](const variable &a, const variable &b) { return a == b; }); break;
    case NEQ: exec_operator([](const variable &a, const variable &b) { return a != b; }); break;
    case AND: exec_operator([](const variable &a, const variable &b) { return a.isTrue() && b.isTrue(); }); break;
    case OR:  exec_operator([](const variable &a, const variable &b) { return a.isTrue() || b.isTrue(); }); break;
    case NOT: m_var_stack.back() = !m_var_stack.back().isTrue(); break;
    case ADD: exec_operator([](const variable &a, const variable &b) { return a + b; }); break;
    case SUB: exec_operator([](const variable &a, const variable &b) { return a - b; }); break;
    case MUL: exec_operator([](const variable &a, const variable &b) { return a * b; }); break;
    case DIV: exec_operator([](const variable &a, const variable &b) { return a / b; }); break;
    case GT:  exec_operator([](const variable &a, const variable &b) { return a > b; }); break;
    case LT:  exec_operator([](const variable &a, const variable &b) { return a < b; }); break;
    case GEQ: exec_operator([](const variable &a, const variable &b) { return a > b || a == b; }); break;
    case LEQ: exec_operator([](const variable &a, const variable &b) { return a < b || a == b; }); break;
    case MAX: exec_operator([](const variable &a, const variable &b) { return a > b ? a : b; }); break;
    case MIN: exec_operator([](const variable &a, const variable &b) { return a < b ? a : b; }); break;
    case SETGLOBAL:
        if (!m_var_stack.back().empty()) {
            m_globals[get_string_ref()] = m_var_stack.back();
        }
        m_var_stack.pop_back();
        break;
    case SETDEBUG: m_var_stack.back().debug = true; break;
    case CLEAR: clear_variable(get_string_ref()); break;
    case APPEND:
        set_variable(get_string_ref(), get_variable_size(get_string_ref()), m_var_stack.back());
        m_var_stack.pop_back();
        break;
    case SETVAR:
        set_variable(get_string_ref(), m_var_stack[m_var_stack.size()-2].asInt(), m_var_stack.back());
        m_var_stack.pop_back();
        m_var_stack.pop_back();
        break;
    case SETVARIDX:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        set_variable(get_string(var_idx.name), var_idx.index, m_var_stack.back());
        m_var_stack.pop_back();
        break;
    }
    case RESETVAR:
        reset_variable(get_string_ref(), m_var_stack.back());
        m_var_stack.pop_back();
        break;
    case COPYCONTENT: m_var_stack.push_back(m_content_stack.back().view()); break;
    case PUSHNUM: m_var_stack.emplace_back(cmd.get<float>()); break;
    case PUSHSTR: m_var_stack.push_back(get_string_ref()); break;
    case PUSHGLOBAL: m_var_stack.push_back(m_globals[get_string_ref()]); break;
    case PUSHVAR:
        m_var_stack.push_back(get_variable(get_string_ref(), m_var_stack.back().asInt()));
        m_var_stack.pop_back();
        break;
    case PUSHVARIDX:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        m_var_stack.push_back(get_variable(get_string(var_idx.name), var_idx.index));
        break;
    }
    case JMP:
        m_programcounter = cmd.get<jump_address>();
        m_jumped = true;
        break;
    case JZ:
        if (!m_var_stack.back().isTrue()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        m_var_stack.pop_back();
        break;
    case JTE:
        if (m_content_stack.back().token_start == m_content_stack.back().text.size()) {
            m_programcounter = cmd.get<jump_address>();
            m_jumped = true;
        }
        break;
    case INCTOP:
        if (!m_var_stack.back().empty()) {
            const auto &name = get_string_ref();
            size_t index = m_var_stack[m_var_stack.size()-2].asInt();
            set_variable(name, index, get_variable(name, index) + m_var_stack.back());
        }
        m_var_stack.pop_back();
        m_var_stack.pop_back();
        break;
    case INCTOPIDX:
        if (!m_var_stack.back().empty()) {
            const auto &var_idx = cmd.get<variable_idx>();
            const auto &name = get_string(var_idx.name);
            set_variable(name, var_idx.index, get_variable(name, var_idx.index) + m_var_stack.back());
        }
        m_var_stack.pop_back();
        break;
    case INC:
    {
        const auto &name = get_string_ref();
        size_t index = m_var_stack.back().asInt();
        set_variable(name, index, get_variable(name, index) + variable(1));
        m_var_stack.pop_back();
        break;
    }
    case INCIDX:
    {
        auto var_idx = cmd.get<variable_idx>();
        const auto &name = get_string(var_idx.name);
        set_variable(name, var_idx.index, get_variable(name, var_idx.index) + variable(1));
        break;
    }
    case INCGTOP:
        if (!m_var_stack.back().empty()) {
            auto &var = m_globals[get_string_ref()];
            var = var + m_var_stack.back();
        }
        m_var_stack.pop_back();
        break;
    case INCG:
    {
        auto &var = m_globals[get_string_ref()];
        var = var + variable(1);
        break;
    }
    case DECTOP:
        if (!m_var_stack.back().empty()) {
            const std::string &name = get_string_ref();
            size_t index = m_var_stack[m_var_stack.size()-2].asInt();
            set_variable(name, index, get_variable(name, index) - m_var_stack.back());
        }
        m_var_stack.pop_back();
        m_var_stack.pop_back();
        break;
    case DECTOPIDX:
        if (!m_var_stack.back().empty()) {
            auto var_idx = cmd.get<variable_idx>();
            const std::string &name = get_string(var_idx.name);
            set_variable(name, var_idx.index, get_variable(name, var_idx.index) - m_var_stack.back());
        }
        m_var_stack.pop_back();
        break;
    case DEC:
    {
        const auto &name = get_string_ref();
        size_t index = m_var_stack.back().asInt();
        set_variable(name, index, get_variable(name, index) - variable(1));
        m_var_stack.pop_back();
        break;
    }
    case DECIDX:
    {
        const auto &var_idx = cmd.get<variable_idx>();
        const std::string &name = get_string(var_idx.name);
        set_variable(name, var_idx.index, get_variable(name, var_idx.index) - variable(1));
        break;
    }
    case DECGTOP:
        if (!m_var_stack.back().empty()) {
            auto &var = m_globals[get_string_ref()];
            var = var - m_var_stack.back();
        }
        m_var_stack.pop_back();
        break;
    case DECG:
    {
        auto &var = m_globals[get_string_ref()];
        var = var - variable(1);
        break;
    }
    case ISSET: m_var_stack.push_back(get_variable_size(get_string_ref()) != 0); break;
    case SIZE: m_var_stack.push_back(get_variable_size(get_string_ref())); break;
    case PUSHCONTENT:
        m_content_stack.emplace_back(m_var_stack.back().str());
        m_var_stack.pop_back();
        break;
    case POPCONTENT: m_content_stack.pop_back(); break;
    case NEXTLINE: m_content_stack.back().next_token("\n"); break;
    case NEXTTOKEN: m_content_stack.back().next_token("\t\n\v\f\r "); break;
    case SPACER:
    {
        const auto &spacer = cmd.get<command_spacer>();
        m_spacers[get_string(spacer.name)] = {spacer.w, spacer.h};
        break;
    }
    case NEXTPAGE:
        ++m_page_num;
        break;
    case HLT:
        m_programcounter = m_commands.size();
        break;
    }
}

void reader::read_box(const pdf_info &info, layout_box box) {
    for (auto &name : tokenize(box.spacers)) {
        if (name.front() == '*') {
            box.page += get_global(name.substr(1)).asInt();
        } else {
            if (name.size() <= 2 || name.at(name.size()-2) != '.') {
                throw layout_error(fmt::format("Spaziatore {0} incorretto", name));
            }
            bool negative = name.front() == '-';
            auto it = m_spacers.find(negative ? name.substr(1, name.size()-3) : name.substr(0, name.size()-2));
            if (it == m_spacers.end()) continue;
            switch (name.back()) {
            case 'x':
                if (negative) box.x -= it->second.w;
                else box.x += it->second.w;
                break;
            case 'w':
                if (negative) box.w -= it->second.w;
                else box.w += it->second.w;
                break;
            case 'y':
                if (negative) box.y -= it->second.h;
                else box.y += it->second.h;
                break;
            case 'h':
                if (negative) box.h -= it->second.h;
                else box.h += it->second.h;
                break;
            default:
                throw layout_error(fmt::format("Spaziatore {0} incorretto", name));
            }
        }
    }

    m_content_stack.clear();
    m_content_stack.emplace_back(pdf_to_text(info, box));
}

const variable &reader::get_global(const std::string &name) const {
    static const variable VAR_NULL;
    auto it = m_globals.find(name);
    if (it == m_globals.end()) {
        return VAR_NULL;
    } else {
        return it->second;
    }
}

const variable &reader::get_variable(const std::string &name, size_t index) const {
    static const variable VAR_NULL;
    if (m_pages.size() <= m_page_num) {
        return VAR_NULL;
    }
    auto &page = m_pages[m_page_num];
    auto it = page.find(name);
    if (it == page.end() || it->second.size() <= index) {
        return VAR_NULL;
    } else {
        return it->second[index];
    }
}

void reader::set_variable(const std::string &name, size_t index, const variable &value) {
    if (value.empty()) return;

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[name];
    while (var.size() <= index) var.emplace_back();
    var[index] = value;
}

void reader::reset_variable(const std::string &name, const variable &value) {
    if (value.empty()) return;

    while (m_pages.size() <= m_page_num) m_pages.emplace_back();
    auto &page = m_pages[m_page_num];
    auto &var = page[name];
    var.clear();
    var.push_back(value);
}

void reader::clear_variable(const std::string &name) {
    if (m_page_num < m_pages.size()) {
        auto &page = m_pages[m_page_num];
        auto it = page.find(name);
        if (it != page.end()) {
            page.erase(it);
        }
    }
}

size_t reader::get_variable_size(const std::string &name) {
    if (m_pages.size() <= m_page_num) {
        return 0;
    }
    auto &page = m_pages[m_page_num];
    auto it = page.find(name);
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
            if (pair.second.front().debug) {
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
        if (!debug && pair.second.debug) continue;
        globals[pair.first] = pair.second.str();
    }

    return out << root;
}