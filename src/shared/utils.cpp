#include "utils.h"

#include <regex>
#include <algorithm>

std::vector<std::string> read_lines(std::istream &stream) {
    std::string token;
    std::vector<std::string> out;
    while (std::getline(stream, token)) {
        out.push_back(token);
    }
    return out;
};

std::vector<std::string> read_lines(const std::string &str) {
    std::istringstream iss(str);
    return read_lines(iss);
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

std::string &string_tolower(std::string &str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) {
            return std::tolower(c);
        });
    return str;
}

std::string &string_trim(std::string &str) {
    string_replace(str, "\r\n", "\n");
    str.erase(0, str.find_first_not_of("\t\n\v\f\r "));
    str.erase(str.find_last_not_of("\t\n\v\f\r ") + 1);
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

std::string parse_date(const std::string &format, const std::string &value, int index) {
    static const char *MONTHS[] = {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"};

    std::string day = format;
    string_replace(day, "DAY", "([0-9]{2})");
    string_replace(day, "DD", "([0-9]{2})");
    string_replace(day, "MM", "[0-9]{2}");
    string_replace(day, "MONTH", "[a-zA-Z]+");
    string_replace(day, "MON", "[a-zA-Z]{3}");
    string_replace(day, "YEAR", "[0-9]{4}");
    string_replace(day, "YYYY", "[0-9]{4}");
    day = search_regex(day, value, index);

    std::string month = format;
    string_replace(month, "DAY", "[0-9]{2}");
    string_replace(month, "DD", "[0-9]{2}");
    string_replace(month, "MM", "([0-9]{2})");
    string_replace(month, "MONTH", "([a-zA-Z]+)");
    string_replace(month, "MON", "([a-zA-Z]{3})");
    string_replace(month, "YEAR", "[0-9]{4}");
    string_replace(month, "YYYY", "[0-9]{4}");
    month = search_regex(month, value, index);
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

    std::string year = format;
    string_replace(year, "DAY", "[0-9]{2}");
    string_replace(year, "DD", "[0-9]{2}");
    string_replace(year, "MM", "[0-9]{2}");
    string_replace(year, "MONTH", "[a-zA-Z]+");
    string_replace(year, "MON", "[a-zA-Z]{3}");
    string_replace(year, "YEAR", "([0-9]{4})");
    string_replace(year, "YYYY", "([0-9]{4})");
    year = search_regex(year, value, index);

    return day.empty() ? year + "-" + month : year + "-" + month + "-" + day;
}

std::string search_regex(std::string format, const std::string &value, int index) {
    string_replace(format, " ", "[ \\t\\r\\n\\v\\f]+");
    string_replace(format, "$NUMBER", "([0-9\\.,-]+)");
    std::regex expression(format, std::regex::icase);
    std::smatch match;
    if (std::regex_search(value, match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}