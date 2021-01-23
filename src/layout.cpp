#include "layout.h"
#include "bytecode.h"
#include "fixed_point.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <locale>

std::ostream &operator << (std::ostream &output, const bill_layout_script &layout) {
    std::ios orig_state(nullptr);
    orig_state.copyfmt(output);
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(fixed_point::decimal_points);

    output << "### Bill Layout Script\n";
    if (!layout.language_code.empty()) {
        output << "### Language " << layout.language_code << '\n';
    }

    for (auto &box : layout.m_boxes) {
        output << '\n';
        output << "### Box\n";
        output << "### Name " << box->name << '\n';
        output << "### Type " << box_type_strings[static_cast<int>(box->type)] << '\n';
        output << "### Mode " << read_mode_strings[static_cast<int>(box->mode)] << '\n';
        output << "### Page " << (int) box->page << '\n';
        output << "### X " << box->x << '\n';
        output << "### Y " << box->y << '\n';
        output << "### W " << box->w << '\n';
        output << "### H " << box->h << '\n';
        if (!box->goto_label.empty()) {
            output << "### Goto Label " << box->goto_label << '\n';
        }
        if (!box->spacers.empty()) {
            output << "### Spacers\n" << box->spacers << "\n### End Spacers\n";
        }
        if (!box->script.empty()) {
            output << "### Script\n" << box->script << "\n### End Script\n";
        }
        output << "### End Box\n";
    }

    output.copyfmt(orig_state);
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

    try {
        while (getline_clearcr(input, line)) {
            if (line.empty()) continue;
            if (line == "### Box") {
                auto current = std::make_shared<layout_box>();
                bool fail = true;
                while (getline_clearcr(input, line)) {
                    if (line.empty()) continue;
                    if (line == "### End Box") {
                        fail = false;
                        break;
                    } else if (auto suf = suffix(line, "### Name")) {
                        current->name = suf.value;
                    } else if (auto suf = suffix(line, "### Type")) {
                        for (size_t i=0; i<std::size(box_type_strings); ++i) {
                            if (box_type_strings[i] == suf.value) {
                                current->type = static_cast<box_type>(i);
                            }
                        }
                    } else if (auto suf = suffix(line, "### Mode")) {
                        for (size_t i=0; i<std::size(read_mode_strings); ++i) {
                            if (read_mode_strings[i] == suf.value) {
                                current->mode = static_cast<read_mode>(i);
                            }
                        }
                    } else if (auto suf = suffix(line, "### Page")) {
                        current->page = cstoi(suf.value);
                    } else if (auto suf = suffix(line, "### X")) {
                        current->x = cstof(suf.value);
                    } else if (auto suf = suffix(line, "### Y")) {
                        current->y = cstof(suf.value);
                    } else if (auto suf = suffix(line, "### W")) {
                        current->w = cstof(suf.value);
                    } else if (auto suf = suffix(line, "### H")) {
                        current->h = cstof(suf.value);
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
    } catch (const std::invalid_argument &error) {
        throw layout_error("Formato non valido");
    }

    return input;
}