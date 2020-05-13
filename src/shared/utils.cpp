#include "utils.h"

#include <regex>
#include <algorithm>

std::vector<std::string> read_lines(const std::string &str) {
    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> out;
    while (std::getline(iss, token)) {
        out.push_back(token);
    }
    return out;
};

std::vector<std::string> tokenize(const std::string &str) {
    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> out;
    while (iss >> token) {
        out.push_back(token);
    }
    return out;
};

std::string implode(const std::vector<std::string> &vec, const std::string &separator) {
    std::string out;
    for (auto it = vec.begin(); it<vec.end(); ++it) {
        if (it != vec.begin()) {
            out += separator;
        }
        out += *it;
    }
    return out;
};

std::string string_tolower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) {
            return std::tolower(c);
        });
    return str;
}

int string_replace(std::string &str, const std::string &from, const std::string &to) {
    size_t index = 0;
    int count = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, from.size(), to);
        index += to.size();
        ++count;
    }
    return count;
}

std::string parse_number(const std::string &value) {
    std::string out;
    for (size_t i=0; i<value.size(); ++i) {
        if (std::isdigit(value.at(i))) {
            out += value.at(i);
        } else if (value.at(i) == ',') {
            out += '.';
        } else if (value.at(i) == '-') {
            out += '-';
        }
    }
    return out;
}

std::string parse_date(std::string format, const std::string &value) {
    static constexpr const char *MONTHS[] = {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"};

    string_replace(format, "DD", "[0-9]{2}");
    string_replace(format, "MM", "([0-9]{2})");
    bool is_month_str = string_replace(format, "MONTH", "([a-zA-Z]+)") || string_replace(format, "MON", "([a-zA-Z]{3})");
    string_replace(format, "YEAR", "([0-9]{4})");
    std::regex expression(format);
    std::smatch match;
    if (std::regex_match(value, match, expression)) {
        std::string month = match.str(1);
        std::string year = match.str(2);

        if (is_month_str) {
            string_tolower(month);
            for (size_t i=0; i<std::size(MONTHS); ++i) {
                if (month.find(MONTHS[i]) != std::string::npos) {
                    if (i < 9) {
                        month = std::string("0") + std::to_string(i + 1);
                    } else {
                        month = std::to_string(i + 1);
                    }
                    break;
                }
            }
        }

        return month + "/" + year;
    } else {
        return "";
    }
}

std::string search_regex(std::string format, const std::string &value) {
    string_replace(format, "$NUMBER", "([0-9\\.,\\-]+)");
    std::regex expression(format, std::regex_constants::icase);
    std::smatch match;
    if (std::regex_search(value, match, expression)) {
        return match.str(1);
    } else {
        return "";
    }
}