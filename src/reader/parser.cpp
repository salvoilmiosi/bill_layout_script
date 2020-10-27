#include "parser.h"

#include <json/json.h>

#include "../shared/utils.h"

void parser::read_layout(const pdf_info &info, const bill_layout_script &layout) {
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        if (*layout.boxes[i].goto_label) {
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
                box.page += it->second.number().getAsInteger();
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
            case 'X':
            case 'w':
            case 'W':
                if (negative) box.x -= it->second.w;
                else box.x += it->second.w;
                break;
            case 'y':
            case 'Y':
            case 'h':
            case 'H':
                if (negative) box.y -= it->second.h;
                else box.y += it->second.h;
                break;
            default:
                throw parsing_error("Identificatore spaziatore incorretto", name);
            }
        }
    }
    std::vector<std::string> scripts = read_lines(box.script);
    box_content content(box);
    content.text = pdf_to_text(info, box);
    for (auto &script : scripts) {
        execute_line(script, content);
    }
}

void parser::read_script(std::istream &stream, const std::string &text) {
    std::vector<std::string> scripts = read_lines(stream);

    box_content content(text);
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
        break;
    default:
        size_t equals = script.find_first_of('=');
        if (equals == std::string::npos) {
            return add_value(script, content.text);
        } else if (equals > 0) {
            return add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), content));
        } else {
            throw parsing_error("Identificatore vuoto", script);
        }
        break;
    }
    return variable();
};

variable parser::add_value(std::string_view name, variable value) {
    if (!value.empty()) {
        if (name.front() == '%') {
            value = variable(parse_number(value.str()), VALUE_NUMBER);
            name.remove_prefix(1);
        }

        if (name.front() == '_') {
            value.debug = true;
            name.remove_prefix(1);
        }

        if (name.front() == '*') {
            name.remove_prefix(1);
            m_globals[std::string(name)] = value;
        } else {
            size_t reading_page_num = m_globals["PAGE_NUM"].number().getAsInteger();
            while (m_values.size() <= reading_page_num) {
                m_values.emplace_back();
            }

            auto &page = m_values[reading_page_num];
            if (name.back() == '+') {
                name.remove_suffix(1);
                page[std::string(name)].push_back(value);
            } else {
                page[std::string(name)] = {value};
            }
        }
    }
    return value;
}

const variable &parser::get_variable(const std::string &name) const {
    static const variable VAR_EMPTY;
    if (name.front() == '*') {
        if (auto it = m_globals.find(name.substr(1)); it != m_globals.end()) {
            return it->second;
        } else {
            return VAR_EMPTY;
        }
    }
    size_t reading_page_num = 0;
    if (auto it = m_globals.find("PAGE_NUM"); it != m_globals.end()) {
        reading_page_num = it->second.number().getAsInteger();
    }
    if (m_values.size() <= reading_page_num) {
        return VAR_EMPTY;
    }
    auto &page = m_values[reading_page_num];
    size_t open_bracket = name.find('[');
    size_t index = 0;
    if (open_bracket != std::string::npos) {
        size_t end_bracket = name.find(']', open_bracket + 1);
        if (end_bracket == std::string::npos) {
            throw parsing_error("Richiesto ']'", name);
        } else try {
            index = std::stoi(name.substr(open_bracket + 1, end_bracket - open_bracket - 1));
        } catch (std::invalid_argument &) {
            throw parsing_error("Indice non valido", name);
        }
    }
    auto it = page.find(name.substr(0, open_bracket));
    
    if (it == page.end() || it->second.size() <= index) {
        return VAR_EMPTY;
    } else {
        return it->second.at(index);
    }
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

    return out << root << std::endl;
}