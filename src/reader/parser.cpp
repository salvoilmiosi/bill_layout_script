#include "parser.h"

#include <json/json.h>

#include "utils.h"

void parser::read_box(const layout_box &box, const std::string &text) {
    auto add_value = [&](const std::string &name, const variable &value) {
        if (name.empty() || value.empty()) return;
        if (name.at(0) == '%') {
            m_values[name.substr(1)].push_back(parse_number(value.str()));
        } else {
            m_values[name].push_back(value);
        }
    };

    auto add_entry = [&](const std::string &script, const std::string &value) {
        size_t equals = script.find_first_of('=');
        if (equals == std::string::npos) {
            add_value(script, value);
        } else if (equals > 0) {
            add_value(script.substr(0, equals), evaluate(script.substr(equals + 1), value));
        } else {
            throw parsing_error("Identificatore vuoto", script);
        }
    };

    std::vector<std::string> scripts = read_lines(box.script);
    std::vector<std::string> values = tokenize(text);

    switch(box.type) {
    case BOX_SINGLE:
        for (auto &script : scripts) {
            add_entry(script, implode(values));
        }
        break;
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
};

function_parser::function_parser(const std::string &script) : script(script) {
    int brace_start = script.find_first_of('(');
    if (brace_start < 0) throw parsing_error("Expected (", script);
    int brace_end = script.find_last_of(')');
    if (brace_end < 0) throw parsing_error("Expected )", script);
    name = script.substr(1, brace_start - 1);
    const std::string &arg_string = script.substr(brace_start + 1, brace_end - brace_start - 1);
    int arg_start = 0, arg_end;
    int bracecount = 0;
    for (size_t i=0; i<arg_string.size(); ++i) {
        switch(arg_string[i]) {
        case '(':
            ++bracecount;
            break;
        case ')':
            --bracecount;
            if (bracecount < 0) throw parsing_error("Extra )", script);
            break;
        case ',':
            if (bracecount == 0) {
                arg_end = i;
                args.push_back(arg_string.substr(arg_start, arg_end - arg_start));
                arg_start = arg_end + 1;
            }
            break;
        default:
            break;
        }
    }
    args.push_back(arg_string.substr(arg_start));
}

variable parser::evaluate(const std::string &script, const std::string &value) {
    if (!script.empty()) switch(script.at(0)) {
    case '&':
    {
        auto it = m_values.find(script.substr(1));
        if (it == m_values.end()) break;

        return it->second[0];
    }
    case '$':
    {
        function_parser function(script);

        if (function.is("search", 2)) {
            return search_regex(function.args[0], evaluate(function.args[1], value).str());
        } else if (function.is("num")) {
            return variable(parse_number(evaluate(function.args[0], value).str()), VALUE_NUMBER);
        } else if (function.is("date", 2)) {
            return parse_date(function.args[0], evaluate(function.args[1], value).str());
        } else if (function.is("if", 2)) {
            if (evaluate(function.args[0], value) != 0) return evaluate(function.args[1], value);
        } else if (function.is("ifnot", 2)) {
            if (evaluate(function.args[0], value) == 0) return evaluate(function.args[1], value);
        } else if (function.is("not")) {
            return evaluate(function.args[0], value) == 0;
        } else if (function.is("eq", 2)) {
            return evaluate(function.args[0], value) == evaluate(function.args[1], value);
        } else if (function.is("neq", 2)) {
            return evaluate(function.args[0], value) != evaluate(function.args[1], value);
        } else if (function.is("and", 2)) {
            return (evaluate(function.args[0], value) != 0) && (evaluate(function.args[1], value) != 0);
        } else if (function.is("or", 2)) {
            return (evaluate(function.args[0], value) != 0) || (evaluate(function.args[1], value) != 0);
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
        } else {
            throw parsing_error("Funzione non riconosciuta: " + function.name, script);
        }

        break;
    }
    case '%':
        return evaluate(script.substr(1), value).number();
    case '@':
        if (script.size() > 1) throw parsing_error("@ must be alone", script);
        return value;
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
        auto &array = values[pair.first] = Json::arrayValue;
        for (auto &value : pair.second) {
            array.append(value.str());
        }
    }

    return out << root << std::endl;
}