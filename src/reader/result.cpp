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

void result::parse_values(const layout_box &box, const std::string &text) {
    std::vector<std::string> names = tokenize(box.parse_string);
    std::vector<std::string> values = tokenize(text);

    switch(box.type) {
    case BOX_SINGLE:
        parse_entry(names[0], implode(values));
        break;
    case BOX_MULTIPLE:
        for (size_t i = 0; i < names.size(); ++i) {
            parse_entry(names[i], values[i]);
        }
        break;
    case BOX_HGRID:
        for (size_t i = 0; i < values.size();++i) {
            parse_entry(names[i % names.size()], values[i]);
        }
        break;
    case BOX_VGRID:
        for (size_t i = 0; i < values.size();++i) {
            parse_entry(names[i / names.size()], values[i]);
        }
        break;
    }
}

void result::parse_entry(const std::string &name, const std::string &value) {

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
            } else if (value.at(i) == ',' || (value.at(i) == '.' && (i + 3 >= value.size() || !std::isdigit(value.at(i + 3))))) {
                parsed += '.';
            }
        }
        m_values[name.substr(1)].push_back(parsed);
        break;
    default:
        // treat as string
        m_values[name].push_back(value);
        break;
    }
}

std::ostream &result::writeTo(std::ostream &out) const {
    auto escape_slashes = [](std::string str) {
        return str; // TODO
    };

    out << "{";
    for (auto i = m_values.begin(); i != m_values.end(); ++i) {
        if (i != m_values.begin()) {
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