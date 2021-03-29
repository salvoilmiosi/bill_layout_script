#include "fixed_point.h"
#include "utils.h"
#include "intl.h"

#include <iostream>
#include <fstream>

template<template<typename> typename Container>
std::ostream &operator << (std::ostream &output, const bill_layout_script<Container> &layout) {
    output << "### Bill Layout Script\n";
    if (intl::valid_language(layout.language_code)) {
        output << fmt::format("### Language {}\n", intl::language_string(layout.language_code));
    }

    for (auto &box : layout) {
        output << '\n'
            << "### Box " << box.name << '\n'
            << "### Type " << box.type << '\n'
            << "### Mode " << box.mode << '\n';
        if (box.flags) {
            output << "### Flags " << box.flags << '\n';
        }
        output << fmt::format("### Page {}\n", box.page);
        output << fmt::format("### Rect {} {} {} {}\n", box.x, box.y, box.w, box.h);
        if (!box.goto_label.empty()) {
            output << fmt::format("### Goto Label {}\n", box.goto_label);
        }
        if (!box.spacers.empty()) {
            output << "### Spacers\n";
            for (const auto &line : string_split(box.spacers, "\n")) {
                if (line == "### End Spacers") {
                    throw layout_error(fmt::format("In {}:\nInvalido Token End Spacers", box.name.empty() ? std::string("(Box senza nome)") : box.name));
                } else {
                    output << line << '\n';
                }
            }
            output << "### End Spacers\n";
        }
        if (!box.script.empty()) {
            output << "### Script\n";
            for (const auto &line : string_split(box.script, "\n")) {
                if (line == "### End Script") {
                    throw layout_error(fmt::format("In {}:\nInvalido Token End Script", box.name.empty() ? std::string("(Box senza nome)") : box.name));
                } else {
                    output << line << '\n';
                }
            }
            output << "### End Script\n";
        }
        output << "### End Box\n";
    }

    return output;
}

struct suffix {
    suffix(std::string_view line, std::string_view str) {
        if (line.starts_with(str)) {
            value = line.substr(std::min(line.size(), line.find_first_not_of(" \t", str.size())));
            valid = true;
        }
    }
    
    operator bool () const {
        return valid;
    }
    
    std::string_view value;
    bool valid = false;
};

static std::istream &getline_clearcr(std::istream &input, std::string &line) {
    std::getline(input, line);
    std::erase(line, '\r');
    return input;
}

template<template<typename> typename Container>
std::istream &operator >> (std::istream &input, bill_layout_script<Container> &layout) {
    std::string line;

    layout.clear();

    while (getline_clearcr(input, line)) {
        if (line.empty()) continue;
        if (auto suf = suffix(line, "### Box")) {
            layout_box current = {};
            current.name = suf.value;
            bool fail = true;
            while (getline_clearcr(input, line)) {
                if (line.empty()) continue;
                if (line == "### End Box") {
                    fail = false;
                    break;
                } else if (auto suf = suffix(line, "### Type")) {
                    try {
                        current.type = FindEnum<box_type>(suf.value);
                    } catch (std::invalid_argument) {
                        throw layout_error(fmt::format("Token 'Type' non valido: {}", suf.value));
                    }
                } else if (auto suf = suffix(line, "### Mode")) {
                    try {
                        current.mode = FindEnum<read_mode>(suf.value);
                    } catch (std::invalid_argument) {
                        throw layout_error(fmt::format("Token 'Mode' non valido: {}", suf.value));
                    }
                } else if (auto suf = suffix(line, "### Flags")) {
                    std::istringstream ss;
                    ss.rdbuf()->pubsetbuf(const_cast<char *>(suf.value.begin()), suf.value.size());
                    std::string label;
                    while (ss >> label) {
                        try {
                            current.flags |= FindEnum<box_flags>(label);
                        } catch (std::invalid_argument) {
                            throw layout_error(fmt::format("Token 'Flags' non valido: {}", label));
                        }
                    }
                } else if (auto suf = suffix(line, "### Page")) {
                    current.page = string_toint(suf.value);
                } else if (auto suf = suffix(line, "### Rect")) {
                    std::istringstream ss;
                    ss.rdbuf()->pubsetbuf(const_cast<char *>(suf.value.begin()), suf.value.size());
                    ss.imbue(std::locale::classic());
                    ss >> current.x >> current.y >> current.w >> current.h;
                    if (ss.fail()) {
                        throw layout_error("Formato non valido");
                    }
                } else if (auto suf = suffix(line, "### Goto Label")) {
                    current.goto_label = suf.value;
                } else if (line == "### Spacers") {
                    bool fail = true;
                    bool first_line = true; 
                    while (getline_clearcr(input, line)) {
                        if (line == "### End Spacers") {
                            fail = false;
                            break;
                        }
                        if (!first_line) current.spacers += '\n';
                        first_line = false;
                        current.spacers += line;
                    }
                    if (fail) {
                        throw layout_error("Token End Spacers non trovato");
                    }
                } else if (line == "### Script") {
                    bool fail = true;
                    bool first_line = true; 
                    while (getline_clearcr(input, line)) {
                        if (line == "### End Script") {
                            fail = false;
                            break;
                        }
                        if (!first_line) current.script += '\n';
                        first_line = false;
                        current.script += line;
                    }
                    if (fail) {
                        throw layout_error("Token End Script non trovato");
                    }
                } else if (line.front() != '#') {
                    throw layout_error("Token non valido");
                }
            }
            if (fail) {
                throw layout_error("Token End Box non trovato");
            } else {
                layout.push_back(current);
            }
        } else if (auto suf = suffix(line, "### Language")) {
            layout.language_code = intl::language_code(std::string(suf.value));
        } else if (line.front() != '#') {
            throw layout_error("Token non valido");
        }
    }

    return input;
}