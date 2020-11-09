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
    short len = readData<short>(input);
    std::string ret(len, '\0');
    input.read(ret.data(), len);
    return ret;
}

struct command_rdbox : public command_args_base, public layout_box {
    command_rdbox() : command_args_base(RDBOX) {}
};

struct command_call : public command_args_base {
    std::string name;
    int numargs;
    command_call() : command_args_base(CALL) {}
};

struct command_spacer : public command_args_base {
    std::string name;
    box_spacer spacer;
    command_spacer() : command_args_base(SPACER) {}
};

template<typename T>
struct command_args : public command_args_base {
    T data;
    command_args(asm_command cmd, const T &data) : command_args_base(cmd), data(data) {}
};

void reader::read_layout(const pdf_info &info, std::istream &input) {
    commands.clear();
    var_stack.clear();
    content_stack.clear();
    index_reg = 0;
    program_counter = 0;
    jumped = false;

    while (!input.eof()) {
        asm_command cmd = readData<asm_command>(input);
        switch(cmd) {
        case RDBOX:
        {
            auto box = std::make_unique<command_rdbox>();
            box->x = readData<float>(input);
            box->y = readData<float>(input);
            box->w = readData<float>(input);
            box->h = readData<float>(input);
            box->page = readData<int>(input);
            box->mode = readData<read_mode>(input);
            box->type = readData<box_type>(input);
            box->spacers = readData<std::string>(input);
            commands.push_back(std::move(box));
            break;
        }
        case CALL:
        {
            auto call = std::make_unique<command_call>();
            call->name = readData<std::string>(input);
            call->numargs = readData<int>(input);
            commands.push_back(std::move(call));
            break;
        }
        case SPACER:
        {
            auto spacer = std::make_unique<command_spacer>();
            spacer->name = readData<std::string>(input);
            spacer->spacer.w = readData<float>(input);
            spacer->spacer.h = readData<float>(input);
            commands.push_back(std::move(spacer));
            break;
        }
        case NOP:
        case SETDEBUG:
        case PUSHCONTENT:
        case SETINDEX:
        case CONTENTVIEW:
        case NEXTLINE:
        case NEXTTOKEN:
        case POPCONTENT:
            commands.push_back(std::make_unique<command_args_base>(cmd));
            break;
        case SETGLOBAL:
        case CLEAR:
        case APPEND:
        case SETVAR:
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
            commands.push_back(std::make_unique<command_args<std::string>>(cmd, readData<std::string>(input)));
            break;
        case PUSHNUM:
            commands.push_back(std::make_unique<command_args<float>>(cmd, readData<float>(input)));
            break;
        case JMP:
        case JZ:
        case JTE:
            commands.push_back(std::make_unique<command_args<int>>(cmd, readData<int>(input)));
            break;
        }
    }

    while (program_counter < commands.size()) {
        exec_command(info, *commands[program_counter]);
        if (!jumped) {
            ++program_counter;
        } else {
            jumped = false;
        }
    }
}

void reader::exec_command(const pdf_info &info, const command_args_base &cmd) {
    auto get_string = [&]() {
        return dynamic_cast<const command_args<std::string> &>(cmd).data;
    };

    auto get_int = [&]() {
        return dynamic_cast<const command_args<int> &>(cmd).data;
    };

    auto get_float = [&]() {
        return dynamic_cast<const command_args<float> &>(cmd).data;
    };

    switch(cmd.command) {
    case NOP: break;
    case RDBOX: read_box(info, dynamic_cast<const layout_box &>(cmd)); break;
    case CALL:
    {
        auto call = dynamic_cast<const command_call &>(cmd);
        call_function(call.name, call.numargs);
        break;
    }
    case SETGLOBAL:
        if (!var_stack.back().empty()) {
            m_globals[get_string()] = var_stack.back();
        }
        var_stack.pop_back();
        break;
    case SETDEBUG: var_stack.back().debug = true; break;
    case CLEAR: clear_variable(get_string()); break;
    case APPEND:
        index_reg = get_variable_size(get_string());
        set_variable(get_string(), var_stack.back());
        index_reg = 0;
        var_stack.pop_back();
        break;
    case SETVAR:
        set_variable(get_string(), var_stack.back());
        index_reg = 0;
        var_stack.pop_back();
        break;
    case PUSHCONTENT: var_stack.push_back(content_stack.back().view_stack.back()); break;
    case PUSHNUM: var_stack.emplace_back(get_float()); break;
    case PUSHSTR: var_stack.push_back(get_string()); break;
    case PUSHGLOBAL: var_stack.push_back(m_globals[get_string()]); break;
    case PUSHVAR:
        var_stack.push_back(get_variable(get_string()));
        index_reg = 0;
        break;
    case SETINDEX:
        index_reg = var_stack.back().asInt();
        var_stack.pop_back();
        break;
    case JMP:
        program_counter = get_int();
        jumped = true;
        break;
    case JZ:
        if (!var_stack.back().isTrue()) {
            program_counter = get_int();
            jumped = true;
        }
        var_stack.pop_back();
        break;
    case JTE:
        if (content_stack.back().tokenend()) {
            program_counter = get_int();
            jumped = true;
        }
        break;
    case INCTOP:
        if (!var_stack.back().empty()) {
            set_variable(get_string(), get_variable(get_string()) + var_stack.back());
            index_reg = 0;
        }
        var_stack.pop_back();
        break;
    case INC:
        set_variable(get_string(), get_variable(get_string()) + variable(1));
        index_reg = 0;
        break;
    case INCGTOP:
        if (!var_stack.back().empty()) {
            auto &var = m_globals[get_string()];
            var = var + var_stack.back();
        }
        var_stack.pop_back();
        break;
    case INCG:
    {
        auto &var = m_globals[get_string()];
        var = var + variable(1);
        break;
    }
    case DECTOP:
        if (!var_stack.back().empty()) {
            set_variable(get_string(), get_variable(get_string()) - var_stack.back());
            index_reg = 0;
        }
        var_stack.pop_back();
        break;
    case DEC:
        set_variable(get_string(), get_variable(get_string()) - variable(1));
        index_reg = 0;
        break;
    case DECGTOP:
        if (!var_stack.back().empty()) {
            auto &var = m_globals[get_string()];
            var = var - var_stack.back();
        }
        var_stack.pop_back();
        break;
    case DECG:
    {
        auto &var = m_globals[get_string()];
        var = var - variable(1);
        break;
    }
    case ISSET: var_stack.push_back(get_variable_size(get_string()) > 0); break;
    case SIZE: var_stack.push_back(get_variable_size(get_string())); break;
    case CONTENTVIEW:
        content_stack.emplace_back(var_stack.back().str());
        content_stack.back().view_stack.emplace_back(content_stack.back().view_stack.back().begin(), 0);
        var_stack.pop_back();
        break;
    case POPCONTENT: content_stack.pop_back(); break;
    case NEXTLINE:
    {
        auto &content = content_stack.back().view_stack[content_stack.back().view_stack.size()-2];
        auto &view = content_stack.back().view_stack.back();
        size_t starttok = content.find_first_not_of('\n', view.end() - content.begin());
        if (starttok == std::string_view::npos) {
            view = std::string_view(content.end(), 0);
        } else {
            size_t endtok = content.find_first_of('\n', starttok);
            view = content.substr(starttok, endtok - starttok);
        }
        break;
    }
    case NEXTTOKEN:
    {
        auto &content = content_stack.back().view_stack[content_stack.back().view_stack.size()-2];
        auto &view = content_stack.back().view_stack.back();
        size_t starttok = content.find_first_not_of("\t\n\v\f\r ", view.end() - content.begin());
        if (starttok == std::string_view::npos) {
            view = std::string_view(content.end(), 0);
        } else {
            size_t endtok = content.find_first_of("\t\n\v\f\r ", starttok);
            view = content.substr(starttok, endtok - starttok);
        }
        break;
    }
    case SPACER:
    {
        auto spacer = dynamic_cast<const command_spacer &>(cmd);
        m_spacers[spacer.name] = spacer.spacer;
        break;
    }
    }
}

void reader::read_box(const pdf_info &info, layout_box box) {
    for (auto &name : tokenize(box.spacers)) {
        if (name.front() == '*') {
            box.page += get_global(name.substr(1)).asInt();
        } else {
            if (name.size() <= 2 || name.at(name.size()-2) != '.') {
                throw layout_error(fmt::format("Identificatore spaziatore {0} incorretto", name));
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
                throw layout_error(fmt::format("Identificatore spaziatore {0} incorretto", name));
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

const variable &reader::get_variable(const std::string &name) const {
    static const variable VAR_NULL;
    size_t pageidx = get_global("PAGE_NUM").asInt();
    if (m_values.size() <= pageidx) {
        return VAR_NULL;
    }
    auto &page = m_values[pageidx];
    auto it = page.find(name);
    if (it == page.end() || it->second.size() <= index_reg) {
        return VAR_NULL;
    } else {
        return it->second[index_reg];
    }
}

void reader::set_variable(const std::string &name, const variable &value) {
    if (value.empty()) return;

    size_t pageidx = get_global("PAGE_NUM").asInt();
    while (m_values.size() <= pageidx) m_values.emplace_back();
    auto &page = m_values[pageidx];
    auto &var = page[name];
    while (var.size() <= index_reg) var.emplace_back();
    var[index_reg] = value;
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