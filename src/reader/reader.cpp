#include "reader.h"

#include <json/json.h>
#include <fmt/core.h>
#include "../shared/utils.h"

template<typename T> T readData(std::istream &input) {
    T buffer;
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
    return buffer;
}

template<> std::string readData(std::istream &input) {
    uint16_t pos = readData<uint16_t>(input);
    if (pos == 0) {
        return "";
    } else {
        auto retpos = input.tellg();
        input.seekg(pos);
        input.ignore(sizeof(uint8_t));
        uint16_t len = readData<uint16_t>(input);
        std::string ret(len, '\0');
        input.read(ret.data(), len);
        input.seekg(retpos);
        return ret;
    }
}

void reader::read_layout(const pdf_info &info, std::istream &input) {
    commands.clear();
    var_stack.clear();
    content_stack.clear();
    program_counter = 0;
    jumped = false;

    while (!input.eof()) {
        asm_command cmd = static_cast<asm_command>(readData<uint8_t>(input));
        switch(cmd) {
        case RDBOX:
        {
            layout_box box;
            box.type = BOX_RECTANGLE;
            box.mode = static_cast<read_mode>(readData<uint8_t>(input));
            box.page = readData<uint8_t>(input);
            box.spacers = readData<std::string>(input);
            box.x = readData<float>(input);
            box.y = readData<float>(input);
            box.w = readData<float>(input);
            box.h = readData<float>(input);
            commands.emplace_back(RDBOX, std::move(box));
            break;
        }
        case RDPAGE:
        {
            layout_box box;
            box.type = BOX_PAGE;
            box.mode = static_cast<read_mode>(readData<uint8_t>(input));
            box.page = readData<uint8_t>(input);
            box.spacers = readData<std::string>(input);
            commands.emplace_back(RDPAGE, std::move(box));
            break;
        }
        case RDFILE:
        {
            layout_box box;
            box.type = BOX_WHOLE_FILE;
            box.mode = static_cast<read_mode>(readData<uint8_t>(input));
            commands.emplace_back(RDFILE, std::move(box));
            break;
        }
        case CALL:
        {
            command_call call;
            call.name = readData<std::string>(input);
            call.numargs = readData<uint8_t>(input);
            commands.emplace_back(CALL, std::move(call));
            break;
        }
        case SPACER:
        {
            command_spacer spacer;
            spacer.name = readData<std::string>(input);
            spacer.w = readData<float>(input);
            spacer.h = readData<float>(input);
            commands.emplace_back(SPACER, std::move(spacer));
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
            var_idx.name = readData<std::string>(input);
            var_idx.index = readData<uint8_t>(input);
            commands.emplace_back(cmd, std::move(var_idx));
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
            commands.emplace_back(cmd, readData<std::string>(input));
            break;
        case PUSHNUM:
            commands.emplace_back(cmd, readData<float>(input));
            break;
        case JMP:
        case JZ:
        case JTE:
            commands.emplace_back(cmd, readData<uint16_t>(input));
            break;
        case STRDATA:
            input.ignore(readData<uint16_t>(input));
            break;
        default:
            commands.emplace_back(cmd);
        }
    }

    while (program_counter < commands.size()) {
        exec_command(info, commands[program_counter]);
        if (!jumped) {
            ++program_counter;
        } else {
            jumped = false;
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
        auto var = op(var_stack[var_stack.size()-2], var_stack.back());
        var_stack.pop_back();
        var_stack.pop_back();
        var_stack.push_back(var);
    };

    switch(cmd.command) {
    case NOP: break;
    case RDBOX:
    case RDPAGE:
    case RDFILE:
        read_box(info, cmd.get<layout_box>());
        break;
    case CALL: call_function(cmd.get<command_call>()); break;
    case ERROR: throw layout_error(cmd.get<std::string>()); break;
    case PARSENUM:
        if (var_stack.back().type() != VALUE_NUMBER) {
            var_stack.back() = variable(parse_number(var_stack.back().str()), VALUE_NUMBER);
        }
        break;
    case PARSEINT: var_stack.back() = variable(std::to_string(var_stack.back().asInt()), VALUE_NUMBER); break;
    case EQ:  exec_operator([](const variable &a, const variable &b) { return a == b; }); break;
    case NEQ: exec_operator([](const variable &a, const variable &b) { return a != b; }); break;
    case AND: exec_operator([](const variable &a, const variable &b) { return a.isTrue() && b.isTrue(); }); break;
    case OR:  exec_operator([](const variable &a, const variable &b) { return a.isTrue() || b.isTrue(); }); break;
    case NOT: var_stack.back() = !var_stack.back().isTrue(); break;
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
        if (!var_stack.back().empty()) {
            m_globals[cmd.get<std::string>()] = var_stack.back();
        }
        var_stack.pop_back();
        break;
    case SETDEBUG: var_stack.back().debug = true; break;
    case CLEAR: clear_variable(cmd.get<std::string>()); break;
    case APPEND:
        set_variable(variable_idx(cmd.get<std::string>(), get_variable_size(cmd.get<std::string>())), var_stack.back());
        var_stack.pop_back();
        break;
    case SETVAR:
        set_variable(variable_idx(cmd.get<std::string>(), var_stack[var_stack.size()-2].asInt()), var_stack.back());
        var_stack.pop_back();
        var_stack.pop_back();
        break;
    case SETVARIDX:
        set_variable(cmd.get<variable_idx>(), var_stack.back());
        var_stack.pop_back();
        break;
    case RESETVAR:
        reset_variable(cmd.get<std::string>(), var_stack.back());
        var_stack.pop_back();
        break;
    case COPYCONTENT: var_stack.push_back(content_stack.back().view()); break;
    case PUSHNUM: var_stack.emplace_back(cmd.get<float>()); break;
    case PUSHSTR: var_stack.push_back(cmd.get<std::string>()); break;
    case PUSHGLOBAL: var_stack.push_back(m_globals[cmd.get<std::string>()]); break;
    case PUSHVAR:
        var_stack.push_back(get_variable(variable_idx(cmd.get<std::string>(), var_stack.back().asInt())));
        var_stack.pop_back();
        break;
    case PUSHVARIDX:
        var_stack.push_back(get_variable(cmd.get<variable_idx>()));
        break;
    case JMP:
        program_counter = cmd.get<uint16_t>();
        jumped = true;
        break;
    case JZ:
        if (!var_stack.back().isTrue()) {
            program_counter = cmd.get<uint16_t>();
            jumped = true;
        }
        var_stack.pop_back();
        break;
    case JTE:
        if (content_stack.back().token_start == content_stack.back().text.size()) {
            program_counter = cmd.get<uint16_t>();
            jumped = true;
        }
        break;
    case INCTOP:
        if (!var_stack.back().empty()) {
            variable_idx var_idx(cmd.get<std::string>(), var_stack[var_stack.size()-2].asInt());
            set_variable(var_idx, get_variable(var_idx) + var_stack.back());
        }
        var_stack.pop_back();
        var_stack.pop_back();
        break;
    case INCTOPIDX:
        if (!var_stack.back().empty()) {
            variable_idx var_idx = cmd.get<variable_idx>();
            set_variable(var_idx, get_variable(var_idx) + var_stack.back());
        }
        var_stack.pop_back();
        break;
    case INC:
    {
        variable_idx var_idx(cmd.get<std::string>(), var_stack.back().asInt());
        set_variable(var_idx, get_variable(var_idx) + variable(1));
        var_stack.pop_back();
        break;
    }
    case INCIDX:
    {
        variable_idx var_idx = cmd.get<variable_idx>();
        set_variable(var_idx, get_variable(var_idx) + variable(1));
        break;
    }
    case INCGTOP:
        if (!var_stack.back().empty()) {
            auto &var = m_globals[cmd.get<std::string>()];
            var = var + var_stack.back();
        }
        var_stack.pop_back();
        break;
    case INCG:
    {
        auto &var = m_globals[cmd.get<std::string>()];
        var = var + variable(1);
        break;
    }
    case DECTOP:
        if (!var_stack.back().empty()) {
            variable_idx var_idx(cmd.get<std::string>(), var_stack[var_stack.size()-2].asInt());
            set_variable(var_idx, get_variable(var_idx) - var_stack.back());
        }
        var_stack.pop_back();
        var_stack.pop_back();
        break;
    case DECTOPIDX:
        if (!var_stack.back().empty()) {
            variable_idx var_idx = cmd.get<variable_idx>();
            set_variable(var_idx, get_variable(var_idx) - var_stack.back());
        }
        var_stack.pop_back();
        break;
    case DEC:
    {
        variable_idx var_idx(cmd.get<std::string>(), var_stack.back().asInt());
        set_variable(var_idx, get_variable(var_idx) - variable(1));
        var_stack.pop_back();
        break;
    }
    case DECIDX:
    {
        variable_idx var_idx = cmd.get<variable_idx>();
        set_variable(var_idx, get_variable(var_idx) - variable(1));
        break;
    }
    case DECGTOP:
        if (!var_stack.back().empty()) {
            auto &var = m_globals[cmd.get<std::string>()];
            var = var - var_stack.back();
        }
        var_stack.pop_back();
        break;
    case DECG:
    {
        auto &var = m_globals[cmd.get<std::string>()];
        var = var - variable(1);
        break;
    }
    case ISSET: var_stack.push_back(get_variable_size(cmd.get<std::string>()) > 0); break;
    case SIZE: var_stack.push_back(get_variable_size(cmd.get<std::string>())); break;
    case PUSHCONTENT:
        content_stack.emplace_back(var_stack.back().str());
        var_stack.pop_back();
        break;
    case POPCONTENT: content_stack.pop_back(); break;
    case NEXTLINE: content_stack.back().next_token("\n"); break;
    case NEXTTOKEN: content_stack.back().next_token("\t\n\v\f\r "); break;
    case SPACER:
    {
        const auto &spacer = cmd.get<command_spacer>();
        m_spacers[spacer.name] = {spacer.w, spacer.h};
        break;
    }
    case HLT:
        program_counter = commands.size();
        break;
    case STRDATA:
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

    content_stack.clear();
    content_stack.emplace_back(pdf_to_text(info, box));
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

const variable &reader::get_variable(const variable_idx &var) const {
    static const variable VAR_NULL;
    size_t pageidx = get_global("PAGE_NUM").asInt();
    if (m_values.size() <= pageidx) {
        return VAR_NULL;
    }
    auto &page = m_values[pageidx];
    auto it = page.find(var.name);
    if (it == page.end() || it->second.size() <= var.index) {
        return VAR_NULL;
    } else {
        return it->second[var.index];
    }
}

void reader::set_variable(const variable_idx &ref, const variable &value) {
    if (value.empty()) return;

    size_t pageidx = get_global("PAGE_NUM").asInt();
    while (m_values.size() <= pageidx) m_values.emplace_back();
    auto &page = m_values[pageidx];
    auto &var = page[ref.name];
    while (var.size() <= ref.index) var.emplace_back();
    var[ref.index] = value;
}

void reader::reset_variable(const std::string &name, const variable &value) {
    if (value.empty()) return;

    size_t pageidx = get_global("PAGE_NUM").asInt();
    while (m_values.size() <= pageidx) m_values.emplace_back();
    auto &page = m_values[pageidx];
    auto &var = page[name];
    var.clear();
    var.push_back(value);
}

void reader::clear_variable(const std::string &name) {
    size_t pageidx = get_global("PAGE_NUM").asInt();
    if (pageidx < m_values.size()) {
        auto &page = m_values[pageidx];
        auto it = page.find(name);
        if (it != page.end()) {
            page.erase(it);
        }
    }
}

size_t reader::get_variable_size(const std::string &name) {
    size_t pageidx = get_global("PAGE_NUM").asInt();
    if (m_values.size() <= pageidx) {
        return 0;
    }
    auto &page = m_values[pageidx];
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

    for (auto &page : m_values) {
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