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
            output << fmt::format("### Spacers\n{}\n### End Spacers\n", box->spacers);
        }
        if (!box->script.empty()) {
            output << fmt::format("### Script\n{}\n### End Script\n", box->script);
        }
        output << "### End Box\n";
    }

    return output;
}

struct suffix {
    suffix(std::string_view line, std::string_view str) {
        if (line.substr(0, str.size()) == str) {
            value = line.substr(str.size() + 1);
        }
    }
    
    operator bool () const {
        return !value.empty();
    }
    
    std::string value;
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
                    for (size_t i=0; i<std::size(box_type_strings); ++i) {
                        if (box_type_strings[i] == suf.value) {
                            current->type = static_cast<box_type>(i);
                            break;
                        }
                    }
                } else if (auto suf = suffix(line, "### Mode")) {
                    for (size_t i=0; i<std::size(read_mode_strings); ++i) {
                        if (read_mode_strings[i] == suf.value) {
                            current->mode = static_cast<read_mode>(i);
                            break;
                        }
                    }
                } else if (auto suf = suffix(line, "### Rect")) {
                    std::istringstream ss(suf.value);
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
                } else {
                    fail = true;
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