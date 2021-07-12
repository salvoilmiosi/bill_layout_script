#include "layout.h"

#include "fixed_point.h"
#include "utils.h"
#include "svstream.h"

#include <iostream>
#include <fstream>

namespace bls {

    std::ostream &operator << (std::ostream &output, const layout_box_list &layout) {
        output << "### Bill Layout Script\n";
        if (layout.setlayout) {
            output << "### Set Layout\n";
        }

        for (auto &box : layout) {
            output << "\n### Box " << box.name << '\n';
            if (box.mode != read_mode::DEFAULT) {
                output << "### Mode " << enums::to_string(box.mode) << '\n';
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
                for (const auto &line : util::string_split(box.spacers, '\n')) {
                    if (line == "### End Spacers") {
                        throw layout_error(std::format("In {}:\nInvalido Token End Spacers", box.name.empty() ? std::string("(Box senza nome)") : box.name));
                    } else {
                        output << line << '\n';
                    }
                }
                output << "### End Spacers\n";
            }
            if (!box.script.empty()) {
                output << "### Script\n";
                for (const auto &line : util::string_split(box.script, '\n')) {
                    if (line == "### End Script") {
                        throw layout_error(std::format("In {}:\nInvalido Token End Script", box.name.empty() ? std::string("(Box senza nome)") : box.name));
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

    std::istream &operator >> (std::istream &input, layout_box_list &layout) {
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
                    } else if (auto suf = suffix(line, "### Mode")) {
                        try {
                            current.mode = enums::from_string<read_mode>(suf.value);
                        } catch (std::invalid_argument) {
                            throw layout_error(std::format("Token 'Mode' non valido: {}", suf.value));
                        }
                    } else if (auto suf = suffix(line, "### Flags")) {
                        util::isviewstream ss{suf.value};
                        std::string label;
                        while (ss >> label) {
                            try {
                                current.flags.set(enums::from_string<box_flags>(label));
                            } catch (std::invalid_argument) {
                                throw layout_error(std::format("Token 'Flags' non valido: {}", label));
                            }
                        }
                    } else if (auto suf = suffix(line, "### Page")) {
                        current.page = util::string_to<int>(suf.value);
                    } else if (auto suf = suffix(line, "### Rect")) {
                        util::isviewstream ss{suf.value};
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
            } else if (line == "### Set Layout") {
                layout.setlayout = true;
            } else if (line.front() != '#') {
                throw layout_error("Token non valido");
            }
        }

        return input;
    }

}