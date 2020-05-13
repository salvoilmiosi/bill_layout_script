#include "parser.h"

#include <regex>

#include <json/json.h>

value_type variable::type() const {
    return m_type;
}

static std::vector<std::string> read_lines(const std::string &str) {
    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> out;
    while (std::getline(iss, token)) {
        out.push_back(token);
    }
    return out;
};

static std::vector<std::string> tokenize(const std::string &str) {
    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> out;
    while (iss >> token) {
        out.push_back(token);
    }
    return out;
};

static std::string implode(const std::vector<std::string> &vec, const std::string &separator = " ") {
    std::string out;
    for (auto it = vec.begin(); it<vec.end(); ++it) {
        if (it != vec.begin()) {
            out += separator;
        }
        out += *it;
    }
    return out;
};

void parser::read_box(const layout_box &box, const std::string &text) {
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

    bool is(const char *funcname, size_t argc = 1) const;
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

bool function_parser::is(const char *funcname, size_t argc) const {
    if (name == funcname) {
        if (args.size() == argc) return true;
        throw parsing_error(std::string("La funzione ") + funcname + " richiede " + std::to_string(argc) + " argomenti", script);
    }
    return false;
}

variable parser::evaluate(const std::string &script) {
    switch(script.at(0)) {
    case '&':
    {
        auto it = m_values.find(script.substr(1));
        if (it == m_values.end()) return 0;
        if (it->second.empty()) return 0;

        return it->second[0];
    }
    case '$':
    {
        function_parser function(script);

        if (function.is("not"))         return evaluate(function.args[0]) == 0;
        if (function.is("eq", 2))       return evaluate(function.args[0]) == evaluate(function.args[1]);
        if (function.is("neq", 2))      return evaluate(function.args[0]) != evaluate(function.args[1]);
        if (function.is("and", 2))      return (evaluate(function.args[0]) != 0) && (evaluate(function.args[1]) != 0);
        if (function.is("or", 2))       return (evaluate(function.args[0]) != 0) || (evaluate(function.args[1]) != 0);
        if (function.is("contains", 2)) return evaluate(function.args[0]).asString().find(evaluate(function.args[1]).asString()) != std::string::npos;

        return 0;
    }
    default:
        return script;
    }
}

static variable parse_number(const std::string &script) {
    std::string out;
    for (size_t i=0; i<script.size(); ++i) {
        if (std::isdigit(script.at(i))) {
            out += script.at(i);
        } else if (script.at(i) == ',') { // si assume formato italiano
            out += '.';
        } else if (script.at(i) == '-') {
            out += '-';
        }
    }
    return variable(out, VALUE_NUMBER);
}

static std::string string_replace(std::string str, const std::string &from, const std::string &to) {
    size_t index = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, to.size(), to);
        index += to.size();
    }
    return str;
}

void parser::add_entry(const std::string &script, const std::string &value) {
    switch(script.at(0)) {
    case '$':
    {
        function_parser function(script);

        if (function.is("num", 1)) { // $num(identificatore) -- legge i numeri
            m_values[function.args[0]].push_back(parse_number(value));
        } else if (function.is("date", 1)) { // $date(identificatore) -- legge le date
            // TODO parsing date
            add_entry(function.args[0], value);
        } else if (function.is("if", 2)) { // $if(condizione,identificatore) -- attiva il blocco se la condizione è verificata
            if (evaluate(function.args[0]) != 0) {
                add_entry(function.args[1], value);
            }
        } else if (function.is("ifnot", 2)) { // $if(condizione,identificatore) -- attiva il blocco se la condizione non è verificata
            if (evaluate(function.args[0]) == 0) {
                add_entry(function.args[1], value);
            }
        } else if (function.is("add", 1)) { // $add(identificatore) -- calcola la somma di tutti gli oggetti del blocco
            auto &val = m_values[function.args[0]];
            auto new_val = parse_number(value);
            if (val.empty()) val.push_back(new_val);
            else val[0] = val[0].asNumber() + new_val.asNumber();
        } else if (function.is("avg", 2)) { // $avg(identificatore,N) -- calcola la media del blocco tra N oggetti (aggiunge valore/N)
            try {
                auto new_val = parse_number(value).asNumber() / std::stof(function.args[1]);
                auto &val = m_values[function.args[0].substr(1)];
                if (val.empty()) val.push_back(new_val);
                else val[0] = val[0].asNumber() + new_val;
            } catch (std::invalid_argument &) {
                throw parsing_error("Il secondo argomento di avg deve essere un numero", script);
            }
        } else if (function.is("max", 2)) { // $max(identificatore) -- calcola il valore massimo
            auto new_val = parse_number(value);
            auto &val = m_values[function.args[0].substr(1)];
            if (val.empty()) val.push_back(new_val);
            else if(val[0].asNumber() < new_val.asNumber()) val[0] = new_val;
        } else if (function.is("min", 2)) { // $min(identificatore) -- calcola il valore minimo
            auto new_val = parse_number(value);
            auto &val = m_values[function.args[0].substr(1)];
            if (val.empty()) val.push_back(new_val);
            else if(val[0].asNumber() > new_val.asNumber()) val[0] = new_val;
        } else if (function.is("search", 2)) { // $search(regex,identificatore) -- Cerca $NUMBER in valore e lo assegna a identificatore
            std::regex expression(string_replace(function.args[0], "$NUMBER", "([0-9\\.,\\-]+)"), std::regex_constants::icase);
            std::smatch match;
            if (std::regex_search(value, match, expression)) {
                add_entry(function.args[1], match.str());
            }
        }
        break;
    }
    case '%':
        m_values[script.substr(1)].push_back(parse_number(value));
        break;
    default:
        m_values[script].emplace_back(value, VALUE_STRING);
    }
}

std::ostream & operator << (std::ostream &out, const parser &res) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::objectValue;

    for (auto &pair : res.m_values) {
        if(!res.debug && pair.first.at(0) == '#') continue;
        auto &array = values[pair.first] = Json::arrayValue;
        for (auto &value : pair.second) {
            array.append(value.asString());
            // I valori sono passati come stringa per evitare errori di conversione float
        }
    }

    return out << root << std::endl;
}