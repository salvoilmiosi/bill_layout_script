#include "utils.h"

#include <regex>
#include <algorithm>

std::vector<std::string> string_split(const std::string &str, char separator) {
    std::vector<std::string> ret;

    size_t start = 0;
    size_t end = str.find(separator);
    while (end != std::string::npos) {
        ret.push_back(str.substr(start, end-start));
        start = end + 1;
        end = str.find(separator, start);
    }
    ret.push_back(str.substr(start, end));

    return ret;
}

std::string string_join(const std::vector<std::string> &vec, const std::string &separator) {
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

std::string string_trim(std::string str) {
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

std::string string_format(std::string str, const std::vector<std::string> &fmt_args) {
    for (size_t i=0; i<fmt_args.size(); ++i) {
        string_replace(str, fmt::format("{{{}}}", i), fmt_args[i]);
    }
    return str;
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

#ifndef NO_JSON_PARSE_STRING

#include <json/reader.h>

std::string parse_string(std::string_view value) {
    Json::Value out;
    std::string errs;
    Json::CharReaderBuilder builder;
    Json::CharReader *reader = builder.newCharReader();
    if (!reader->parse(value.begin(), value.end(), &out, &errs)) {
        return "";
    }
    if (out.isString()) {
        return out.asString();
    } else {
        return "";
    }
}

#else

std::string parse_string(std::string_view value) {
    std::string decoded;
    auto current = value.begin() + 1;
    auto end = value.end() - 1;
    while (current != end) {
        char c = *current++;
        if (c == '"') break;
        if (c == '\\') {
            if (current == end) return "";
            char escape = *current++;
            switch (escape) {
            case '"':
                decoded += '"';
                break;
            case '/':
                decoded += '/';
                break;
            case '\\':
                decoded += '\\';
                break;
            case 'b':
                decoded += '\b';
                break;
            case 'f':
                decoded += '\f';
                break;
            case 'n':
                decoded += '\n';
                break;
            case 'r':
                decoded += '\r';
                break;
            case 't':
                decoded += '\t';
                break;
            default:
                return std::string();
            }
        } else {
            decoded += c;
        }
    }
    return decoded;
}

#endif

template<typename ... Ts>
constexpr bool find_in (const char *str, const char *first, const Ts & ... strs) {
    if (str == first) {
        return true;
    } else if constexpr (sizeof ... (strs) == 0) {
        return false;
    } else {
        return find_in(str, strs...);
    }
}

constexpr const char *MONTHS[] = {
    "gen",
    "feb",
    "mar",
    "apr",
    "mag",
    "giu",
    "lug",
    "ago",
    "set",
    "ott",
    "nov",
    "dic"
};

constexpr const char *MONTHS_FULL[] = {
    "gennaio",
    "febbraio",
    "marzo",
    "aprile",
    "maggio",
    "giugno",
    "luglio",
    "agosto",
    "settembre",
    "ottobre",
    "novembre",
    "dicembre"
};

std::string parse_date(const std::string &format, const std::string &value, int index) {
    auto replace = [](std::string out, const auto& ... strs) {
        auto replace_impl = [&](std::string &out, const char *str, const std::string &fmt) {
            if (find_in(str, strs...)) {
                string_replace(out, str, "(" + fmt + ")");
            } else {
                string_replace(out, str, fmt);
            }
        };

        replace_impl(out, "DAY", "[0-9]{2}");
        replace_impl(out, "DD", "[0-9]{2}");
        replace_impl(out, "MM", "[0-9]{2}");
        replace_impl(out, "MONTH", "[a-zA-Z]+");
        replace_impl(out, "MON", "[a-zA-Z]{3}");
        replace_impl(out, "YEAR", "[0-9]{4}");
        replace_impl(out, "YYYY", "[0-9]{4}");
        replace_impl(out, "YY", "[0-9]{2}");

        return out;
    };

    std::string day = search_regex(replace(format, "DAY", "DD"), value, index);
    std::string month = string_tolower(search_regex(replace(format, "MM", "MONTH", "MON"), value, index));
    std::string year = search_regex(replace(format, "YEAR", "YYYY", "YY"), value, index);

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

    try {
        int yy = std::stoi(year);
        if (yy < 100) {
            if (yy > 90) {
                year = "19" + year;
            } else {
                year = "20" + year;
            }
        }
    } catch (const std::invalid_argument &) {}

    if (year.empty() || month.empty()) {
        return "";
    } else if (day.empty()) {
        return year + "-" + month;
    } else {
        return year + "-" + month + "-" + day;
    }
}

std::string date_month_add(const std::string &date, int num) {
    try {
        std::regex expression("(\\d{4})-(\\d{2})(-(\\d{2}))?");
        std::smatch match;
        if (!std::regex_search(date, match, expression))
            return "";

        int month = std::stoi(match.str(2));
        int year = std::stoi(match.str(1));

        month += num;
        while (month > 12) {
            month -= 12;
            ++year;
        }
        return fmt::format("{:04}-{:02}", year, month);
    } catch (std::invalid_argument &) {
        return "";
    }
}

std::string date_format(const std::string &date, std::string format) {
    std::regex expression("(\\d{4})-(\\d{2})(-(\\d{2}))?");
    std::smatch match;
    if (!std::regex_search(date, match, expression))
        return "";
    
    int month = std::stoi(match.str(2));
    if (month > (int) std::size(MONTHS)) {
        return "";
    }

    string_replace(format, "DAY", match.str(4));
    string_replace(format, "DD", match.str(4));
    string_replace(format, "MM", match.str(2));
    string_replace(format, "MONTH", MONTHS_FULL[month-1]);
    string_replace(format, "MON", MONTHS[month-1]);
    string_replace(format, "YEAR", match.str(1));
    string_replace(format, "YYYY", match.str(1));
    string_replace(format, "YY", match.str(1).substr(2));

    return format;
}

std::vector<std::string> search_regex_all(std::string format, std::string value, int index) {
    string_replace(format, " ", "[ \\t\\r\\n\\v\\f]+");
    string_replace(format, "$NUMBER", "([0-9\\.,-]+)");
    std::regex expression(format, std::regex::icase);
    std::smatch match;
    std::vector<std::string> ret;
    while (std::regex_search(value, match, expression)) {
        ret.push_back(match.str(index));
        value = match.suffix();
    }
    return ret;
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

std::string nospace(std::string input) {
    size_t index = 0;
    while (true) {
        index = input.find_first_of("\t\r\n\v\f", index);
        if (index == std::string::npos) break;

        input.replace(index, 1, " ");
        ++index;
    }
    return input;
}