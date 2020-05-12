#include "parser.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <string_view>
#include <regex>

#include <json/json.h>

var_value& var_value::operator = (const var_value &other) {
    if (other.m_type != m_type && m_type != VALUE_UNDEFINED) {
        throw parsing_error(std::string("Can't set a ") + TYPES[other.m_type] + " to a " + TYPES[m_type], "");
    }
    m_type = other.m_type;
    if (other.m_type == VALUE_NUMBER && m_type != VALUE_UNDEFINED) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::min(precision(), other.precision())) << other.asNumber();
        m_value = stream.str();
    } else {
        m_value = other.m_value;
    }
    return *this;
}

bool var_value::operator == (const var_value &other) const {
    switch(other.m_type) {
    case VALUE_STRING:
        return m_value == other.m_value;
    case VALUE_NUMBER:
        return asNumber() == other.asNumber();
    default:
        return false;
    }
}

bool var_value::operator != (const var_value &other) const {
    return !(*this == other);
}

int var_value::precision() const {
    if (m_type == VALUE_NUMBER) {
        int dot_pos = m_value.find_last_of('.');
        if (dot_pos >= 0) {
            return m_value.size() - dot_pos - 1;
        } else {
            return 0;
        }
    }
    return 0;
}

const std::string &var_value::asString() const {
    return m_value;
}

float var_value::asNumber() const {
    switch(m_type) {
    case VALUE_STRING:
        return m_value.empty() ? 1 : 0;
    case VALUE_NUMBER:
        try {
            return std::stof(m_value);
        } catch (std::invalid_argument &) {
            return 0;
        }
    default:
        return 0;
    }
}

value_type var_value::type() const {
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
    std::vector<std::string> names = read_lines(box.parse_string);
    std::vector<std::string> values = tokenize(text);

    switch(box.type) {
    case BOX_SINGLE:
        for (auto &name : names) {
            add_entry(name, implode(values));
        }
        break;
    case BOX_MULTIPLE:
        for (size_t i = 0; i < names.size() && i < values.size(); ++i) {
            add_entry(names[i], values[i]);
        }
        break;
    case BOX_COLUMNS:
        for (size_t i = 0; i < values.size();++i) {
            add_entry(names[i % names.size()], values[i]);
        }
        break;
    case BOX_ROWS:
        for (size_t i = 0; i < values.size();++i) {
            int row_len = values.size() / names.size();
            if (row_len < 1) row_len = 1;
            if (i / row_len < names.size()) {
                add_entry(names[i / row_len], values[i]);
            }
        }
        break;
    }
}

const std::vector<var_value> &parser::get_value(const std::string &name) {
    static const std::vector<var_value> value_empty;
    auto it = m_values.find(name);
    if (it == m_values.end()) {
        return value_empty;
    } else {
        return it->second;
    }
}

var_value parser::evaluate(const std::string &value) {
    switch(value.at(0)) {
    case '&':
    {
        auto it = m_values.find(value.substr(1));
        if (it == m_values.end()) return 0;
        if (it->second.empty()) return 0;

        return it->second[0];
    }
    case '$':
        return parse_function(value);
    default:
        return value;
    }
}

struct function_tokens {
    const std::string &value;

    std::string name;
    std::vector<std::string> args;

    function_tokens(const std::string &value);

    bool is(const char *funcname, size_t argc = 1) const;
};

function_tokens::function_tokens(const std::string &value) : value(value) {
    int brace_start = value.find_first_of('(');
    if (brace_start < 0) throw parsing_error("Expected (", value);
    int brace_end = value.find_last_of(')');
    if (brace_end < 0) throw parsing_error("Expected )", value);
    name = value.substr(1, brace_start - 1);
    const std::string &arg_string = value.substr(brace_start + 1, brace_end - brace_start - 1);
    int arg_start = 0, arg_end;
    int bracecount = 0;
    for (size_t i=0; i<arg_string.size(); ++i) {
        switch(arg_string[i]) {
        case '(':
            ++bracecount;
            break;
        case ')':
            --bracecount;
            if (bracecount < 0) throw parsing_error("Extra )", value);
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

bool function_tokens::is(const char *funcname, size_t argc) const {
    if (name == funcname) {
        if (args.size() == argc) return true;
        throw parsing_error(std::string("La funzione ") + funcname + " richiede " + std::to_string(argc) + " argomenti", value);
    }
    return false;
}

var_value parser::parse_function(const std::string &value) {
    function_tokens tokens(value);
    if (tokens.is("not")) {
        return evaluate(tokens.args[0]) == 0;
    } else if (tokens.is("eq", 2)) {
        return evaluate(tokens.args[0]) == evaluate(tokens.args[1]);
    } else if (tokens.is("neq", 2)) {
        return evaluate(tokens.args[0]) != evaluate(tokens.args[1]);
    } else if (tokens.is("and", 2)) {
        return (evaluate(tokens.args[0]) != 0) && (evaluate(tokens.args[1]) != 0);
    } else if (tokens.is("or", 2)) {
        return (evaluate(tokens.args[0]) != 0) || (evaluate(tokens.args[1]) != 0);
    }
    return 0;
}

var_value parser::parse_number(const std::string &value) {
    std::string out;
    for (size_t i=0; i<value.size(); ++i) {
        if (std::isdigit(value.at(i))) {
            out += value.at(i);
        } else if (value.at(i) == ',') { // si assume formato italiano
            out += '.';
        } else if (value.at(i) == '-') {
            out += '-';
        }
    }
    return var_value(out, VALUE_NUMBER);
}

static std::string string_replace_occurrences(std::string str, const std::string &from, const std::string &to) {
    size_t index = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, to.size(), to);
        index += to.size();
    }
    return str;
}

void parser::search_value(const std::string &regex, const std::string &name, const std::string &value) {
    std::regex expression(string_replace_occurrences(regex, "$NUMBER", "([0-9\\.,\\-]+)"), std::regex_constants::icase);
    std::smatch match;
    if (std::regex_search(value, match, expression)) {
        add_entry(name, match.str());
    }
}

void parser::top_function(const std::string &name, const std::string &value) {
    function_tokens tokens(name);
    if (tokens.is("date", 1)) { // $date(identificatore) -- legge le date
        // TODO parsing date
        add_entry(tokens.args[0], value);
    } else if (tokens.is("if", 2)) { // $if(condizione,identificatore) -- attiva il blocco se la condizione e' verificata
        if (evaluate(tokens.args[0]) != 0) {
            add_entry(tokens.args[1], value);
        }
    } else if (tokens.is("add", 1)) { // $add(%identificatore) -- calcola la somma di tutti gli oggetti del blocco
        if (tokens.args[0].at(0) != '%') throw parsing_error("La funzione add richiede un identificatore numerico", name);
        auto &val = m_values[tokens.args[0].substr(1)];
        auto new_val = parse_number(value);
        if (val.empty()) val.push_back(new_val);
        else val[0] = val[0].asNumber() + new_val.asNumber();
    } else if (tokens.is("avg", 2)) { // $avg(%identificatore,N) -- calcola la media del blocco tra N oggetti (aggiunge valore/N)
        if (tokens.args[0].at(0) != '%') throw parsing_error("La funzione avg richiede un identificatore numerico", name);
        try {
            auto new_val = parse_number(value).asNumber() / std::stof(tokens.args[1]);
            auto &val = m_values[tokens.args[0].substr(1)];
            if (val.empty()) val.push_back(new_val);
            else val[0] = val[0].asNumber() + new_val;
        } catch (std::invalid_argument &) {
            throw parsing_error("Il secondo argomento di avg deve essere un numero", name);
        }
    } else if (tokens.is("max", 2)) { // $max(%identificatore) -- calcola il valore massimo
        if (tokens.args[0].at(0) != '%') throw parsing_error("La funzione max richiede un identificatore numerico", name);
        auto new_val = parse_number(value);
        auto &val = m_values[tokens.args[0].substr(1)];
        if (val.empty()) val.push_back(new_val);
        else if(val[0].asNumber() < new_val.asNumber()) val[0] = new_val;
    } else if (tokens.is("min", 2)) { // $min(%identificatore) -- calcola il valore minimo
        if (name.at(0) != '%') throw parsing_error("La funzione min richiede un identificatore numerico", name);
        auto new_val = parse_number(value);
        auto &val = m_values[tokens.args[0].substr(1)];
        if (val.empty()) val.push_back(new_val);
        else if(val[0].asNumber() > new_val.asNumber()) val[0] = new_val;
    } else if (tokens.is("search", 2)) { // $search(regex,identificatore) -- Cerca $NUMBER in valore e lo assegna a identificatore
        search_value(tokens.args[0], tokens.args[1], value);
    }
}

void parser::add_entry(const std::string &name, const std::string &value) {
    switch (name.at(0)) {
    case '#':
        // skip
        break;
    case '%':
        // treat as number
        m_values[name.substr(1)].push_back(parse_number(value));
        break;
    case '$':
        // treat as function
        top_function(name, value);
        break;
    default:
        // treat as string
        m_values[name].emplace_back(value, VALUE_STRING);
        break;
    }
}

std::ostream & operator << (std::ostream &out, const parser &res) {
    Json::Value root = Json::objectValue;

    Json::Value &values = root["values"] = Json::objectValue;

    for (auto &pair : res.m_values) {
        auto &array = values[pair.first] = Json::arrayValue;
        for (auto &value : pair.second) {
            array.append(value.asString());
            // I valori sono passati come stringa per evitare errori di conversione float
        }
    }

    return out << root << std::endl;
}