#include "functions.h"
#include "utils.h"

#include <regex>
#include <algorithm>
#include <fmt/format.h>

std::string string_format(std::string str, const std::vector<std::string> &fmt_args) {
    for (size_t i=0; i<fmt_args.size(); ++i) {
        string_replace(str, fmt::format("${}", i), fmt_args[i]);
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

struct date_t {
    int year = 0;
    int month = 0;
    int day = 0;
};

bool date_from_string(const std::string &str, date_t &date) {
    std::regex expression("(\\d{4})-(\\d{2})(-(\\d{2}))?");
    std::smatch match;
    if (std::regex_search(str, match, expression)) {
        date.year = std::stoi(match.str(1));
        date.month = std::stoi(match.str(2));
        if (date.month > 12) {
            return false;
        }
        if (match.size() >= 4) {
            date.day = std::stoi(match.str(3));
        }
        return true;
    }
    return false;
}

std::string date_to_string(const date_t &date) {
    if (date.year == 0 || date.month == 0) {
        return "";
    } else if (date.day == 0) {
        return fmt::format("{:04}-{:02}", date.year, date.month);
    } else {
        return fmt::format("{:04}-{:02}-{:02}", date.year, date.month, date.day);
    }
}

template<typename ... Ts>
constexpr bool find_variadic(const char *str, const char *first, const Ts & ... strs) {
    if (str == first) {
        return true;
    } else if constexpr (sizeof ... (strs) == 0) {
        return false;
    } else {
        return find_variadic(str, strs...);
    }
}

std::string parse_date(const std::string &format, const std::string &value, int index) {
    auto replace = [](std::string out, const auto& ... strs) {
        auto replace_impl = [&](std::string &out, const char *str, const std::string &fmt) {
            if (find_variadic(str, strs...)) {
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
        date_t date;
        date.year = std::stoi(year);
        date.month = std::stoi(month);
        if (!day.empty()) date.day = std::stoi(day);
        if (date.year < 100) {
            if (date.year > 90) {
                date.year += 1900;
            } else {
                date.year += 2000;
            }
        }
        return date_to_string(date);
    } catch (const std::invalid_argument &) {
        return "";
    }
}

std::string date_month_add(const std::string &str, int num) {
    date_t date;
    if (!date_from_string(str, date)) {
        return "";
    }

    date.month += num;
    while (date.month > 12) {
        date.month -= 12;
        ++date.year;
    }
    return date_to_string(date);
}

std::string date_format(const std::string &str, std::string format) {
    date_t date;
    if (!date_from_string(str, date)) {
        return "";
    }

    string_replace(format, "DAY", std::to_string(date.day));
    string_replace(format, "DD", std::to_string(date.day));
    string_replace(format, "MM", std::to_string(date.month));
    string_replace(format, "MONTH", MONTHS_FULL[date.month-1]);
    string_replace(format, "MON", MONTHS[date.month-1]);
    string_replace(format, "YEAR", std::to_string(date.year));
    string_replace(format, "YYYY", std::to_string(date.year));
    string_replace(format, "YY", std::to_string(date.year).substr(2));

    return format;
}

static const std::regex &create_regex(const std::string &format) {
    static std::map<std::string, std::regex> expressions;
    if (expressions.find(format) == expressions.end()) {
        std::string format_edit;
        string_replace(format_edit, " ", "\\s+");
        string_replace(format_edit, "%N", "[0-9\\.,-]+");
        expressions[format] = std::regex(format_edit, std::regex::icase);
    }
    return expressions[format];
}

std::vector<std::string> search_regex_all(const std::string &format, std::string value, int index) {
    const std::regex &expression = create_regex(format);
    std::smatch match;
    std::vector<std::string> ret;
    while (std::regex_search(value, match, expression)) {
        ret.push_back(match.str(index));
        value = match.suffix();
    }
    return ret;
}

std::string search_regex(const std::string &format, const std::string &value, int index) {
    const std::regex &expression = create_regex(format);
    std::smatch match;
    if (std::regex_search(value, match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}

std::string nonewline(std::string input) {
    size_t index = 0;
    while (true) {
        index = input.find_first_of("\t\r\n\v\f", index);
        if (index == std::string::npos) break;

        input.replace(index, 1, " ");
        ++index;
    }
    return input;
}