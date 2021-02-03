#include "layout.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "utils.h"

#include <iostream>
#include <fstream>

std::ostream &operator << (std::ostream &output, const bill_layout_script &layout) {
    output << "### Bill Layout Script\n";
    if (!layout.language_code.empty()) {
        output << fmt::format("### Language {}\n", layout.language_code);
    }

    for (auto &box : layout.m_boxes) {
        output << fmt::format("\n### Box {}\n", box->name);
        output << fmt::format("### Type {}\n", box_type_strings[int(box->type)]);
        output << fmt::format("### Mode {}\n", read_mode_strings[int(box->mode)]);
        output << fmt::format("### Rect {} {} {} {} {}\n", box->page, box->x, box->y, box->w, box->h);\
        if (!box->goto_label.empty()) {
            output << fmt::format("### Goto Label {}\n", box->goto_label);
        }
        if (!box->spacers.empty()) {
            output << "### Spacers\n";
            for (auto &line : string_split(box->spacers, '\n')) {
                if (line == "### End Spacers") {
                    throw layout_error(fmt::format("In {}:\nInvalido Token End Spacers", box->name.empty() ? std::string("(Box senza nome)") : box->name));
                } else {
                    output << line << '\n';
                }
            }
            output << "### End Spacers\n";
        }
        if (!box->script.empty()) {
            output << "### Script\n";
            for (auto &line : string_split(box->script, '\n')) {
                if (line == "### End Script") {
                    throw layout_error(fmt::format("In {}:\nInvalido Token End Script", box->name.empty() ? std::string("(Box senza nome)") : box->name));
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

std::istream &getline_clearcr(std::istream &input, std::string &line) {
    auto &ret = std::getline(input, line);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    return ret;
}

std::istream &operator >> (std::istream &input, bill_layout_script &layout) {
    std::string line;

    layout.clear();
    std::shared_ptr<layout_box> current;

    while (getline_clearcr(input, line)) {
        if (line.empty()) continue;
        if (auto suf = suffix(line, "### Box")) {
            auto current = std::make_shared<layout_box>();
            current->name = suf.value;
            bool fail = true;
            while (getline_clearcr(input, line)) {
                if (line.empty()) continue;
                if (line == "### End Box") {
                    fail = false;
                    break;
                } else if (auto suf = suffix(line, "### Type")) {
                    auto it = std::find(std::begin(box_type_strings), std::end(box_type_strings), suf.value);
                    if (it != std::end(box_type_strings)) {
                        current->type = static_cast<box_type>(it - box_type_strings);
                    } else {
                        throw layout_error(fmt::format("Token 'Type' non valido: {}", suf.value));
                    }
                } else if (auto suf = suffix(line, "### Mode")) {
                    auto it = std::find(std::begin(read_mode_strings), std::end(read_mode_strings), suf.value);
                    if (it != std::end(read_mode_strings)) {
                        current->mode = static_cast<read_mode>(it - read_mode_strings);
                    } else {
                        throw layout_error(fmt::format("Token 'Mode' non valido: {}", suf.value));
                    }
                } else if (auto suf = suffix(line, "### Rect")) {
                    std::istringstream ss(std::string(suf.value));
                    ss.imbue(std::locale::classic());
                    ss >> current->page >> current->x >> current->y >> current->w >> current->h;
                    if (ss.fail()) {
                        throw layout_error("Formato non valido");
                    }
                } else if (auto suf = suffix(line, "### Goto Label")) {
                    current->goto_label = suf.value;
                } else if (line == "### Spacers") {
                    bool fail = true;
                    bool first_line = true; 
                    while (getline_clearcr(input, line)) {
                        if (line == "### End Spacers") {
                            fail = false;
                            break;
                        }
                        if (!first_line) current->spacers += '\n';
                        first_line = false;
                        current->spacers += line;
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
                        if (!first_line) current->script += '\n';
                        first_line = false;
                        current->script += line;
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
                layout.m_boxes.push_back(current);
            }
        } else if (auto suf = suffix(line, "### Language")) {
            layout.language_code = suf.value;
        } else if (line.front() != '#') {
            throw layout_error("Token non valido");
        }
    }

    return input;
}