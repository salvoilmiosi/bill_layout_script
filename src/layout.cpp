#include "layout.h"

#include "fixed_point.h"

#include <iostream>
#include <fstream>

using namespace bls;
using namespace enums::stream_operators;

void layout_box_list::save_file() {
    std::ofstream output(filename);
    if (!output) {
        throw file_error(intl::translate("CANT_SAVE_FILE", filename.string()));
    }

    output << "### Bill Layout Script\n";
    if (find_layout_flag) {
        output << "### Flag Find Layout\n";
    }
    if (!language.empty()) {
        output << "### Language " << language << '\n';
    }

    for (const auto &box : *this) {
        output << "\n### Box " << box.name << '\n';
        if (box.mode != read_mode::DEFAULT) {
            output << "### Mode " << box.mode << '\n';
        }
        if (!box.flags.empty()) {
            output << "### Flags " << box.flags << '\n';
        }
        output << std::format("### Page {}\n", box.page);
        output << std::format("### Rect {} {} {} {}\n", box.x, box.y, box.w, box.h);
        if (!box.goto_label.empty()) {
            output << std::format("### Goto Label {}\n", box.goto_label);
        }
        if (!box.spacers.empty()) {
            output << "### Spacers\n";
            for (std::string_view line : util::string_split(box.spacers, '\n')) {
                if (line == "### End Spacers") {
                    throw parsing_error(intl::translate("INVALID_TOKEN_END_SPACERS"));
                } else {
                    output << line << '\n';
                }
            }
            output << "### End Spacers\n";
        }
        if (!box.script.empty()) {
            output << "### Script\n";
            for (std::string_view line : util::string_split(box.script, '\n')) {
                if (line == "### End Script") {
                    throw parsing_error(intl::translate("INVALID_TOKEN_END_SCRIPT"));
                } else {
                    output << line << '\n';
                }
            }
            output << "### End Script\n";
        }
        output << "### End Box\n";
    }
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

layout_box_list::layout_box_list(const std::filesystem::path &filename) : filename(filename) {
    std::ifstream input(filename);
    if (!input) {
        throw file_error(intl::translate("CANT_OPEN_FILE", filename.string()));
    }

    std::string line;
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
                } else if (auto suf = suffix(line, "### Mode")) {
                    if (auto value = enums::from_string<read_mode>(suf.value)) {
                        current.mode = *value;
                    } else {
                        throw parsing_error(intl::translate("INVALID_TOKEN_MODE", suf.value));
                    }
                } else if (auto suf = suffix(line, "### Flags")) {
                    util::isviewstream ss{suf.value};
                    std::string label;
                    while (ss >> label) {
                        if (auto value = enums::from_string<box_flags>(label)) {
                            current.flags.set(*value);
                        } else {
                            throw parsing_error(intl::translate("INVALID_TOKEN_FLAGS", label));
                        }
                    }
                } else if (auto suf = suffix(line, "### Page")) {
                    current.page = util::string_to<int>(suf.value);
                } else if (auto suf = suffix(line, "### Rect")) {
                    util::isviewstream ss{suf.value};
                    ss >> current.x >> current.y >> current.w >> current.h;
                    if (ss.fail()) {
                        throw parsing_error(intl::translate("INVALID_TOKEN_RECT"));
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
                        throw parsing_error(intl::translate("TOKEN_END_SPACERS_NOT_FOUND"));
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
                        throw parsing_error(intl::translate("TOKEN_END_SCRIPT_NOT_FOUND"));
                    }
                } else if (line.front() != '#') {
                    throw parsing_error(intl::translate("INVALID_TOKEN", line));
                }
            }
            if (fail) {
                throw parsing_error(intl::translate("TOKEN_END_BOX_NOT_FOUND"));
            } else {
                push_back(current);
            }
        } else if (auto suf = suffix(line, "### Language")) {
            language = suf.value;
        } else if (line == "### Flag Find Layout") {
            find_layout_flag = true;
        } else if (line.front() != '#') {
            throw parsing_error(intl::translate("INVALID_TOKEN", line));
        }
    }
}