#include "result.h"

#include <iostream>
#include <sstream>
#include <algorithm>

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

void result::read_box(const layout_box &box, const std::string &text) {
    std::vector<std::string> names = tokenize(box.parse_string);
    std::vector<std::string> values = tokenize(text);

    switch(box.type) {
    case BOX_SINGLE:
        add_entry(names[0], implode(values));
        break;
    case BOX_MULTIPLE:
        for (size_t i = 0; i < names.size(); ++i) {
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
            add_entry(names[i / (values.size() / names.size())], values[i]);
        }
        break;
    }
}

std::string result::evaluate(const std::string &value) {
    switch(value.at(0)) {
    case '%':
    {
        auto it = m_values.find(value.substr(1));
        if (it == m_values.end()) return "0";
        if (it->second.empty()) return "0";

        return it->second[0];
    }
    case '$':
        return parse_function(value);
    default:
        return value;
    }
}

std::string result::parse_function(const std::string &value) {
    int brace_start = value.find_first_of('(');
    int brace_end = value.find_last_of(')');
    std::string func_name = value.substr(1, brace_start - 1);
    std::string args = value.substr(brace_start + 1, brace_end - brace_start - 1);
    if (func_name == "not") {
        if (std::stof(evaluate(args)) == 0.f) {
            return "1";
        }
        return "0";
    } else {
        return "0";
    }
}

void result::top_function(const std::string &name, const std::string &value) {
    int brace_start = name.find_first_of('(');
    int brace_end = name.find_last_of(')');
    std::string func_name = name.substr(1, brace_start - 1);
    std::string args = name.substr(brace_start + 1, brace_end - brace_start - 1);
    if (func_name == "date") {
        // TODO parsing date
        add_entry(args, value);
    } else if (func_name == "if") {
        int comma = args.find_first_of(',');
        std::string arg0 = args.substr(0, comma);
        std::string arg1 = args.substr(comma + 1);
        if (std::stof(evaluate(arg0)) != 0.f) {
            add_entry(arg1, value);
        }
    }
}

void result::add_entry(const std::string &name, const std::string &value) {
    std::string parsed;
    switch (name.at(0)) {
    case '#':
        // skip
        break;
    case '%':
        // treat as number
        for (size_t i=0; i<value.size(); ++i) {
            if (std::isdigit(value.at(i))) {
                parsed += value.at(i);
            } else if (value.at(i) == ',') { // si assume formato italiano
                parsed += '.';
            }
        }
        m_values[name.substr(1)].push_back(parsed);
        break;
    case '$':
        // treat as function
        top_function(name, value);
        break;
    default:
        // treat as string
        m_values[name].push_back(value);
        break;
    }
}

std::ostream & operator<<(std::ostream &out, const result &res) {
    auto escape_slashes = [](std::string str) {
        return str; // TODO
    };

    out << "{";
    for (auto i = res.m_values.begin(); i != res.m_values.end(); ++i) {
        if (i != res.m_values.begin()) {
            out << ", ";
        }
        out << '"' << i->first << "\" : [";
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            if (j != i->second.begin()) {
                out << ", ";
            }
            out << '"' << escape_slashes(*j) << '"';
        }
        out << ']';
    }

    out << '}';

    return out;
}