#include "layout.h"

#include <iostream>

constexpr const char *BOX_TYPES[] = {
    "RECTANGLE",
    "PAGE",
    "FILE",
    "DISABLED"
};

constexpr const char *READ_MODES[] = {
    "DEFAULT",
    "LAYOUT",
    "RAW"
};

std::ostream &operator << (std::ostream &output, const bill_layout_script &layout) {
    output << "### Bill Layout Script\n";

    for (auto &box : layout) {
        output << '\n';
        output << "### Box\n";
        output << "### Name " << box->name << '\n';
        output << "### Type " << BOX_TYPES[box->type] << '\n';
        output << "### Mode " << READ_MODES[box->mode] << '\n';
        output << "### Page " << box->page << '\n';
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

    return output;
}

struct suffix {
    suffix(const std::string &line, const std::string &str) {
        if (line.substr(0, str.size()) == str) {
            value = line.substr(str.size() + 1);
        }
    }
    
    operator bool () const {
        return !value.empty();
    }
    
    std::string value;
};

std::istream &operator >> (std::istream &input, bill_layout_script &layout) {
    std::string line;
    if (!std::getline(input, line) || line != "### Bill Layout Script") {
        input.setstate(std::ios::failbit);
        return input;
    }

    layout.clear();
    std::shared_ptr<layout_box> current;

    try {
        while (std::getline(input, line)) {
            if (line.empty()) continue;
            if (line == "### Box") {
                auto current = std::make_shared<layout_box>();
                bool fail = true;
                while (std::getline(input, line)) {
                    if (line.empty()) continue;
                    if (line == "### End Box") {
                        fail = false;
                        break;
                    } else if (auto suf = suffix(line, "### Name")) {
                        current->name = suf.value;
                    } else if (auto suf = suffix(line, "### Type")) {
                        for (size_t i=0; i<std::size(BOX_TYPES); ++i) {
                            if (BOX_TYPES[i] == suf.value) {
                                current->type = static_cast<box_type>(i);
                            }
                        }
                    } else if (auto suf = suffix(line, "### Mode")) {
                        for (size_t i=0; i<std::size(READ_MODES); ++i) {
                            if (READ_MODES[i] == suf.value) {
                                current->mode = static_cast<read_mode>(i);
                            }
                        }
                    } else if (auto suf = suffix(line, "### Page")) {
                        current->page = std::stoi(suf.value);
                    } else if (auto suf = suffix(line, "### X")) {
                        current->x = std::stof(suf.value);
                    } else if (auto suf = suffix(line, "### Y")) {
                        current->y = std::stof(suf.value);
                    } else if (auto suf = suffix(line, "### W")) {
                        current->w = std::stof(suf.value);
                    } else if (auto suf = suffix(line, "### H")) {
                        current->h = std::stof(suf.value);
                    } else if (auto suf = suffix(line, "### Goto Label")) {
                        current->goto_label = suf.value;
                    } else if (line == "### Spacers") {
                        bool fail = true;
                        while (std::getline(input, line)) {
                            if (line == "### End Spacers") {
                                fail = false;
                                break;
                            }
                            if (!current->spacers.empty()) current->spacers += '\n';
                            current->spacers += line;
                        }
                        if (fail) {
                            input.setstate(std::ios::failbit);
                            return input;
                        }
                    } else if (line == "### Script") {
                        bool fail = true;
                        while (std::getline(input, line)) {
                            if (line == "### End Script") {
                                fail = false;
                                break;
                            }
                            if (!current->script.empty()) current->script += '\n';
                            current->script += line;
                        }
                        if (fail) {
                            input.setstate(std::ios::failbit);
                            return input;
                        }
                    }
                }
                if (fail) {
                    input.setstate(std::ios::failbit);
                    return input;
                } else {
                    layout.push_back(current);
                }
            } else {
                input.setstate(std::ios::failbit);
                return input;
            }
        }
    } catch (std::invalid_argument &) {
        input.setstate(std::ios::failbit);
    }

    return input;
}