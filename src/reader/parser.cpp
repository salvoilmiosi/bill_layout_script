#include "parser.h"

#include <json/json.h>

#include "../shared/utils.h"

void parser::read_layout(const pdf_info &info, const bill_layout_script &layout) {
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        if (!layout.boxes[i].goto_label.empty()) {
            goto_labels[layout.boxes[i].goto_label] = i;
        }
    }
    program_counter = 0;
    while (program_counter < layout.boxes.size()) {
        read_box(info, layout.boxes[program_counter]);
        if (jumped) {
            jumped = false;
        } else {
            ++program_counter;
        }
    }
}

void parser::read_box(const pdf_info &info, layout_box box) {
    for (auto &name : tokenize(box.spacers)) {
        if (name.front() == '*') {
            if (auto it = m_globals.find(name.substr(1)); it != m_globals.end()) {
                box.page += it->second.asInt();
            }
        } else {
            if (name.size() <= 2 || name.at(name.size()-2) != '.') {
                throw parsing_error("Identificatore spaziatore incorretto", name);
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
                throw parsing_error("Identificatore spaziatore incorretto", name);
            }
        }
    }
    execute_script(box_content(box, pdf_to_text(info, box)));
}

void parser::execute_script(const box_content &content) {
    std::vector<std::string> scripts;

    size_t start = 0;
    size_t end = 0;
    while (end != std::string::npos) {
        if (content.script[start] == '#') {
            end = content.script.find('\n', start);
        } else {
            end = content.script.find(';', start);
            scripts.push_back(string_trim(nospace(content.script.substr(start, end - start))));
        }
        start = end + 1;
    }

    for (auto &script : scripts) {
        execute_line(script, content);
    }
}

variable parser::execute_line(const std::string &script, const box_content &content) {
    if (script.empty()) return variable();

    switch (script.front()) {
    case '#':
        break;
    case '$':
        return evaluate(script, content);
    default:
        size_t equals = script.find_first_of('=');
        if (equals == std::string::npos) {
            return add_value(script, content.text, content);
        } else if (equals > 0) {
            return add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), content), content);
        } else {
            throw parsing_error("Identificatore vuoto", script);
        }
        break;
    }
    return variable();
};

variable parser::add_value(std::string_view name, variable value, const box_content &content) {
    if (name.front() == '%') {
        if (value.type() != VALUE_NUMBER) {
            value = variable(parse_number(value.str()), VALUE_NUMBER);
        }
        name.remove_prefix(1);
    }

    if (name.front() == '!') {
        value.debug = true;
        name.remove_prefix(1);
    }

    if (!value.empty()) {
        *get_variable(name, content) = value;
    }
    return value;
}

const variable &parser::get_global(const std::string &name) const {
    static const variable VAR_EMPTY;

    auto it = m_globals.find(name);
    if (it == m_globals.end()) {
        return VAR_EMPTY;
    } else {
        return it->second;
    }
}

variable &variable_ref::operator *() {
    if (index == INDEX_GLOBAL) {
        return parent.m_globals[name];
    }

    while (parent.m_values.size() <= pageidx) parent.m_values.emplace_back();

    auto &var = parent.m_values[pageidx][name];
    switch (index) {
    case INDEX_CLEAR:
        var.clear();
        index = 0;
        break;
    case INDEX_APPEND:
        index = var.size();
        break;
    }
    while (var.size() <= index) var.emplace_back();

    return var[index];
}

void variable_ref::clear() {
    if (parent.m_values.size() <= pageidx) return;

    if (index == INDEX_GLOBAL) {
        parent.m_globals.erase(name);
    } else {
        parent.m_values[pageidx].erase(name);
    }
}

size_t variable_ref::size() const {
    if (parent.m_values.size() <= pageidx) return 0;

    if (index == INDEX_GLOBAL) {
        return parent.m_globals.find(name) != parent.m_globals.end();
    }

    auto &page = parent.m_values[pageidx];
    auto it = page.find(name);
    if (it == page.end()) return 0;
    return it->second.size();
}

bool variable_ref::isset() const {
    if (parent.m_values.size() <= pageidx) return false;

    if (index == INDEX_GLOBAL) {
        return parent.m_globals.find(name) != parent.m_globals.end();
    }

    auto &page = parent.m_values[pageidx];
    auto it = page.find(name);
    if (it == page.end()) return false;

    if (index < 0 || index >= it->second.size()) return false;

    return true;
}

variable_ref parser::get_variable(std::string_view name, const box_content &content) {
    variable_ref ref(*this);

    if (name.front() == '*') {
        name.remove_prefix(1);
        ref.name = name;
        ref.index = variable_ref::INDEX_GLOBAL;
        return ref;
    }

    size_t open_bracket = name.find('[');
    if (open_bracket == std::string::npos) {
        switch (name.back()) {
        case '+':
            name.remove_suffix(1);
            ref.index = variable_ref::INDEX_APPEND;
            break;
        case ':':
            name.remove_suffix(1);
            ref.index = variable_ref::INDEX_CLEAR;
            break;
        }
    } else {
        size_t end_bracket = name.find(']', open_bracket + 1);
        if (end_bracket == std::string::npos) {
            throw parsing_error("Richiesto ']'", std::string(name));
        } else {
            ref.index = evaluate(std::string(name.substr(open_bracket + 1, end_bracket - open_bracket - 1)), content).asInt();
            name.remove_suffix(name.size() - open_bracket);
        }
    }

    ref.pageidx = get_global("PAGE_NUM").asInt();
    ref.name = name;

    return ref;
}

std::ostream & operator << (std::ostream &out, const parser &res) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::arrayValue;

    for (auto &page : res.m_values) {
        auto &page_values = values.append(Json::objectValue);
        for (auto &pair : page) {
            if(!res.debug && pair.second.front().debug) continue;
            auto &json_arr = page_values[pair.first] = Json::arrayValue;
            for (auto &val : pair.second) {
                json_arr.append(val.str());
            }
        }
    }

    Json::Value &globals = root["globals"] = Json::objectValue;
    for (auto &pair : res.m_globals) {
        if (!res.debug && pair.second.debug) continue;
        globals[pair.first] = pair.second.str();
    }

    return out << root << std::endl;
}