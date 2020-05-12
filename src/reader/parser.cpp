#include "parser.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <string_view>

#include <json/json.h>

var_value::var_value() {}

var_value& var_value::operator = (const var_value &other) {
    if (other.m_type != m_type && m_type != VALUE_UNDEFINED) {
        throw parsing_error(std::string("Can't set a ") + TYPES[other.m_type] + " to a " + TYPES[m_type], "");
    }
    m_type = other.m_type;
    m_value = other.m_value;
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
    std::vector<std::string> names = tokenize(box.parse_string);
    std::vector<std::string> values = tokenize(text);

    switch(box.type) {
    case BOX_SINGLE:
        add_entry(names[0], implode(values));
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

var_value parser::evaluate(std::string_view value) {
    switch(value.at(0)) {
    case '%':
    {
        auto it = m_values.find(std::string(value.substr(1)));
        if (it == m_values.end()) return 0;
        if (it->second.empty()) return 0;

        return it->second[0];
    }
    case '$':
        return parse_function(value);
    default:
        return std::string(value);
    }
}

struct function_tokens {
    std::string_view name;
    std::vector<std::string_view> args;

    function_tokens(std::string_view value);
};

function_tokens::function_tokens(std::string_view value) {
    int brace_start = value.find_first_of('(');
    if (brace_start < 0) throw parsing_error("Expected (", std::string(value));
    int brace_end = value.find_last_of(')');
    if (brace_end < 0) throw parsing_error("Expected )", std::string(value));
    name = value.substr(1, brace_start - 1);
    std::string_view arg_string = value.substr(brace_start + 1, brace_end - brace_start - 1);
    int arg_start = 0, arg_end;
    int bracecount = 0;
    for (size_t i=0; i<arg_string.size(); ++i) {
        switch(arg_string[i]) {
        case '(':
            ++bracecount;
            break;
        case ')':
            --bracecount;
            if (bracecount < 0) throw parsing_error("Extra )", std::string(value));
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

var_value parser::parse_function(std::string_view value) {
    function_tokens tokens(value);
    if (tokens.name == "not") {
        if (evaluate(tokens.args[0]) == 0) {
            return 1;
        }
        return 0;
    } else {
        return 0;
    }
}

var_value parser::parse_number(std::string_view value) {
    std::string out;
    for (size_t i=0; i<value.size(); ++i) {
        if (std::isdigit(value.at(i))) {
            out += value.at(i);
        } else if (value.at(i) == ',') { // si assume formato italiano
            out += '.';
        }
    }
    return var_value(out, VALUE_NUMBER);
}

void parser::top_function(std::string_view name, std::string_view value) {
    function_tokens tokens(name);
    if (tokens.name == "date") { // $date(identificatore) -- legge le date
        // TODO parsing date
        add_entry(tokens.args[0], value);
    } else if (tokens.name == "if") { // $if(condizione,identificatore) -- attiva il blocco se la condizione e' verificato
        if (tokens.args.size() != 2) throw parsing_error("La funzione if ha due argomenti", std::string(name));
        if (evaluate(tokens.args[0]) != 0) {
            add_entry(tokens.args[1], value);
        }
    } else if (tokens.name == "add") { // $add(%identificatore) -- calcola la somma di tutti gli oggetti del blocco
        if (tokens.args[0].at(0) != '%') throw parsing_error("Un identificatore numerico deve iniziare con %", std::string(name));
        auto &val = m_values[std::string(tokens.args[0].substr(1))];
        auto new_val = parse_number(value);
        if (val.empty()) val.push_back(new_val);
        else val[0] = val[0].asNumber() + new_val.asNumber();
    } else if (tokens.name == "avg") { // $avg(%identificatore,N) -- calcola la media del blocco tra N oggetti (aggiunge valore/N)
        if (tokens.args.size() != 2) throw parsing_error("La funzione avg ha due argomenti", std::string(name));
        if (tokens.args[0].at(0) != '%') throw parsing_error("Un identificatore numerico deve iniziare con %", std::string(name));
        try {
            auto new_val = parse_number(value).asNumber() / std::stof(std::string(tokens.args[1]));
            auto &val = m_values[std::string(tokens.args[0].substr(1))];
            if (val.empty()) val.push_back(new_val);
            else val[0] = val[0].asNumber() + new_val;
        } catch (std::invalid_argument &) {
            throw parsing_error("Il secondo argomento di avg deve essere un numero", std::string(name));
        }
    } else if (tokens.name == "max") { // $max(%identificatore) -- calcola il valore massimo
        if (tokens.args[0].at(0) != '%') throw parsing_error("Un identificatore numerico deve iniziare con %", std::string(name));
        auto new_val = parse_number(value);
        auto &val = m_values[std::string(tokens.args[0].substr(1))];
        if (val.empty()) val.push_back(new_val);
        else if(val[0].asNumber() < new_val.asNumber()) val[0] = new_val;
    } else if (tokens.name == "min") { // $min(%identificatore) -- calcola il valore minimo
        if (name.at(0) != '%') throw parsing_error("Un identificatore numerico deve iniziare con %", std::string(name));
        auto new_val = parse_number(value);
        auto &val = m_values[std::string(tokens.args[0].substr(1))];
        if (val.empty()) val.push_back(new_val);
        else if(val[0].asNumber() > new_val.asNumber()) val[0] = new_val;
    }
}

void parser::add_entry(std::string_view name, std::string_view value) {
    switch (name.at(0)) {
    case '#':
        // skip
        break;
    case '%':
        // treat as number
        m_values[std::string(name.substr(1))].push_back(parse_number(value));
        break;
    case '$':
        // treat as function
        top_function(name, value);
        break;
    default:
        // treat as string
        m_values[std::string(name)].emplace_back(value, VALUE_STRING);
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