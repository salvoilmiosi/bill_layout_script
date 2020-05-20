#include "parser.h"

#include <json/json.h>

#include "../shared/utils.h"

void parser::add_value(const std::string &name, const variable &value) {
    if (name.empty() || value.empty()) return;
    if (name.at(name.size()-1) == '+') {
        if (name.at(0) == '%') {
            m_values[name.substr(1, name.size()-2)].emplace_back(parse_number(value.str()), VALUE_NUMBER);
        } else {
            m_values[name.substr(0, name.size()-1)].push_back(value);
        }
    } else {
        if (name.at(0) == '%') {
            m_values[name.substr(1)] = {variable(parse_number(value.str()), VALUE_NUMBER)};
        } else {
            m_values[name] = {value};
        }
    }
}

void parser::add_entry(const std::string &script, const std::string &value) {
    size_t equals = script.find_first_of('=');
    if (equals == std::string::npos) {
        add_value(script, value);
    } else if (equals > 0) {
        add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), value));
    } else {
        throw parsing_error("Identificatore vuoto", script);
    }
};

void parser::add_spacer(const std::string &script, const std::string &value, const spacer &size) {
    size_t equals = script.find_first_of('=');
    if (equals == std::string::npos) {
        throw parsing_error("Errore di sintassi", script);
    } else if (equals > 0) {
        if (evaluate(script.substr(equals + 1), value)) {
            m_spacers.emplace(script.substr(0, equals), size);
        }
    } else {
        throw parsing_error("Identificatore vuoto", script);
    }
}

void parser::read_box(const std::string &app_dir, const std::string &file_pdf, const pdf_info &info, const layout_box &box) {
    pdf_rect box_moved = box;
    for (auto &name : tokenize(box.spacers)) {
        if (name.size() <= 2 || name.at(name.size()-2) != '.') {
            throw parsing_error("Identificatore spaziatore incorretto", name);
        }
        bool negative = name.at(0) == '-';
        auto it = m_spacers.find(negative ? name.substr(1, name.size()-3) : name.substr(0, name.size()-2));
        if (it == m_spacers.end()) continue;
        switch (name.at(name.size()-1)) {
        case 'x':
        case 'X':
        case 'w':
        case 'W':
            if (negative) box_moved.x -= it->second.w;
            else box_moved.x += it->second.w;
            break;
        case 'y':
        case 'Y':
        case 'h':
        case 'H':
            if (negative) box_moved.y -= it->second.h;
            else box_moved.y += it->second.h;
            break;
        default:
            throw parsing_error("Identificatore spaziatore incorretto", name);
        }
    }
    std::vector<std::string> scripts = read_lines(box.script);
    if (box.type == BOX_WHOLE_FILE) {
        std::string text = pdf_whole_file_to_text(app_dir, file_pdf);
        for (auto &script : scripts) {
            add_entry(script, text);
        }
    } else {
        std::string text = pdf_to_text(app_dir, file_pdf, info, box_moved);
        if (box.type == BOX_SINGLE) {
            for (auto &script : scripts) {
                add_entry(script, text);
            }
        } else {
            std::vector<std::string> values = tokenize(text);
            switch(box.type) {
            case BOX_MULTIPLE:
                for (size_t i = 0; i < scripts.size() && i < values.size(); ++i) {
                    add_entry(scripts[i], values[i]);
                }
                break;
            case BOX_COLUMNS:
                for (size_t i = 0; i < values.size();++i) {
                    add_entry(scripts[i % scripts.size()], values[i]);
                }
                break;
            case BOX_ROWS:
                for (size_t i = 0; i < values.size();++i) {
                    int row_len = values.size() / scripts.size();
                    if (row_len < 1) row_len = 1;
                    if (i / row_len < scripts.size()) {
                        add_entry(scripts[i / row_len], values[i]);
                    }
                }
                break;
            case BOX_SPACER:
                for (auto &script : scripts) {
                    add_spacer(script, text, spacer(box.w, box.h));
                }
                break;
            default:
                break;
            }
        }
    }
}

void parser::read_script(std::istream &stream, const std::string &text) {
    std::vector<std::string> scripts = read_lines(stream);

    for (auto &script : scripts) {
        add_entry(script, text);
    }
}

const variable &parser::get_variable(const std::string &name, size_t index) const {
    static const variable VAR_EMPTY;
    auto it = m_values.find(name);
    if (it == m_values.end() || it->second.size() <= index) {
        return VAR_EMPTY;
    } else {
        return it->second.at(index);
    }
}

struct function_parser {
    const std::string &script;

    std::string name;
    std::vector<std::string> args;

    function_parser(const std::string &script);

    bool is(const char *funcname, size_t argc = 1) const {
        if (name == funcname) {
            if (args.size() == argc) return true;
            throw parsing_error(std::string("La funzione ") + funcname + " richiede " + std::to_string(argc) + " argomenti", script);
        }
        return false;
    }

    bool isleast(const char *funcname, size_t argc = 1) const {
        if (name == funcname) {
            if (args.size() >= argc) return true;
            throw parsing_error(std::string("La funzione ") + funcname + " richiede " + std::to_string(argc) + " argomenti", script);
        }
        return false;
    }
};

function_parser::function_parser(const std::string &script) : script(script) {
    size_t brace_start = script.find_first_of('(');
    if (brace_start == std::string::npos) throw parsing_error("Previsto '('", script);
    size_t brace_end = script.find_last_of(')');
    if (brace_end  == std::string::npos) throw parsing_error("Previsto ')'", script);
    name = script.substr(1, brace_start - 1);

    std::string reading_arg;
    int bracecount = 0;
    bool string_flag = false;
    bool escape_flag = false;
    for (size_t i = brace_start; i <= brace_end; ++i) {
        if (string_flag) {
            if (escape_flag) {
                reading_arg += script[i];
                escape_flag = false;
            } else switch(script[i]) {
            case '"':
                string_flag = false;
                break;
            case '\\':
                escape_flag = true;
                break;
            default:
                reading_arg += script[i];
            }
        } else {
            switch(script[i]) {
            case '"':
                string_flag = true;
                break;
            case '(':
                ++bracecount;
                if (bracecount != 1) {
                    reading_arg += '(';
                }
                break;
            case ')':
                --bracecount;
                if (bracecount == 0) {
                    args.push_back(reading_arg);
                    if (i < brace_end) {
                        throw parsing_error("Valori non previsti dopo ')'", script);
                    }
                } else {
                    reading_arg += ')';
                }
                break;
            case ',':
                if (bracecount == 1) {
                    args.push_back(reading_arg);
                    reading_arg.clear();
                } else {
                    reading_arg += ',';
                }
                break;
            default:
                reading_arg += script[i];
                break;
            }
        }
    }
}

variable parser::evaluate(const std::string &script, const std::string &value) {
    if (!script.empty()) switch(script.at(0)) {
    case '$':
    {
        function_parser function(script);

        if (function.isleast("search", 2)) {
            return search_regex(evaluate(function.args[0], value).str(), evaluate(function.args[1], value).str(),
                function.args.size() >= 3 ? evaluate(function.args[2], value).number().getAsInteger() : 1);
        } else if (function.is("num")) {
            return variable(parse_number(evaluate(function.args[0], value).str()), VALUE_NUMBER);
        } else if (function.isleast("date", 2)) {
            return parse_date(evaluate(function.args[0], value).str(), evaluate(function.args[1], value).str(),
                function.args.size() >= 3 ? evaluate(function.args[2], value).number().getAsInteger() : 1);
        } else if (function.is("month_begin", 1)) {
            return evaluate(function.args[0], value).str() + "-01";
        } else if (function.is("month_end", 1)) {
            return date_month_end(evaluate(function.args[0], value).str());
        } else if (function.is("if", 2)) {
            if (evaluate(function.args[0], value)) return evaluate(function.args[1], value);
        } else if (function.is("ifnot", 2)) {
            if (!evaluate(function.args[0], value)) return evaluate(function.args[1], value);
        } else if (function.is("not")) {
            return !evaluate(function.args[0], value);
        } else if (function.is("eq", 2)) {
            return evaluate(function.args[0], value) == evaluate(function.args[1], value);
        } else if (function.is("neq", 2)) {
            return evaluate(function.args[0], value) != evaluate(function.args[1], value);
        } else if (function.is("and", 2)) {
            return evaluate(function.args[0], value) && evaluate(function.args[1], value);
        } else if (function.is("or", 2)) {
            return evaluate(function.args[0], value) || evaluate(function.args[1], value);
        } else if (function.is("contains", 2)) {
            return evaluate(function.args[0], value).str().find(evaluate(function.args[1], value).str()) != std::string::npos;
        } else if (function.is("add", 2)) {
            return evaluate(function.args[0], value) + evaluate(function.args[1], value);
        } else if (function.is("sub", 2)) {
            return evaluate(function.args[0], value) - evaluate(function.args[1], value);
        } else if (function.is("mul", 2)) {
            return evaluate(function.args[0], value) * evaluate(function.args[1], value);
        } else if (function.is("div", 2)) {
            return evaluate(function.args[0], value) / evaluate(function.args[1], value);
        } else if (function.is("gt", 2)) {
            return evaluate(function.args[0], value) > evaluate(function.args[0], value);
        } else if (function.is("lt", 2)) {
            return evaluate(function.args[0], value) < evaluate(function.args[1], value);
        } else if (function.is("max", 2)) {
            return std::max(evaluate(function.args[0], value), evaluate(function.args[1], value));
        } else if (function.is("min", 2)) {
            return std::min(evaluate(function.args[0], value), evaluate(function.args[1], value));
        } else if (function.is("cat", 2)) {
            return evaluate(function.args[0], value).str() + evaluate(function.args[1], value).str();
        } else {
            throw parsing_error("Funzione non riconosciuta: " + function.name, script);
        }

        break;
    }
    case '&':
        return get_variable(script.substr(1));
    case '@':
        return value + script.substr(1);
    default:
        return script;
    }

    return variable();
}

std::ostream & operator << (std::ostream &out, const parser &res) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::objectValue;

    for (auto &pair : res.m_values) {
        if(!res.debug && pair.first.at(0) == '#') continue;
        auto &json_arr = values[pair.first] = Json::arrayValue;
        for (auto &val : pair.second) {
            json_arr.append(val.str());
        }
    }

    return out << root << std::endl;
}