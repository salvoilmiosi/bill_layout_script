#include "functions.h"
#include "utils.h"
#include "layout.h"
#include "locale_ita.h"

#include <regex>
#include <algorithm>
#include <fmt/format.h>

std::string parse_number(const std::string &str) {
    std::string out;
    for (char c : str) {
        if (std::isdigit(c) || c == '-') {
            out += c;
        } else if (c == DECIMAL_POINT) {
            out += '.';
        }
    }
    return out;
}

std::string string_format(std::string str, const std::vector<std::string> &fmt_args) {
    for (size_t i=0; i<fmt_args.size(); ++i) {
        string_replace(str, fmt::format("${}", i), fmt_args[i]);
    }
    return str;
}

struct date_t {
    int year = 0;
    int month = 0;
    int day = 0;
};

bool date_from_string(const std::string &str, date_t &date) {
    static std::regex expression("^(\\d{4})-(\\d{2})(-(\\d{2}))?$");
    try {
        std::smatch match;
        if (std::regex_search(str, match, expression)) {
            date.year = std::stoi(match.str(1));
            date.month = std::stoi(match.str(2));
            if (date.month > 12) {
                return false;
            }
            auto day = match.str(4);
            if (!day.empty()) date.day = std::stoi(day);
            return true;
        }
    } catch (std::invalid_argument &) {}
    return false;
}

std::string date_to_string(const date_t &date) {
    if (!date.year || !date.month) {
        return "";
    } else if (!date.day) {
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
    std::string month = search_regex(replace(format, "MM"), value, index);
    std::string month_str = string_tolower(search_regex(replace(format, "MONTH", "MON"), value, index));
    std::string year = search_regex(replace(format, "YEAR", "YYYY", "YY"), value, index);

    try {
        date_t date;
        if (!month.empty()) {
            date.month = std::stoi(month);
        } else if (!month_str.empty()) {
            if (auto it = std::find_if(std::begin(MONTHS_ABBR), std::end(MONTHS_ABBR), [month_str](auto &str) {
                return month_str.find(str) != std::string::npos;
            }); it != std::end(MONTHS_ABBR)) {
                date.month = it - std::begin(MONTHS_ABBR) + 1;
            }
        } 
        if (!day.empty()) date.day = std::stoi(day);
        if (!year.empty()) date.year = std::stoi(year);

        if (date.year < 100) {
            if (date.year > 90) {
                date.year += 1900;
            } else {
                date.year += 2000;
            }
        }
        return date_to_string(date);
    } catch (std::invalid_argument &) {
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

    auto to_str = [](int n, int size) { return fmt::format("{:0{}}", n, size); };

    string_replace(format, "DAY", to_str(date.day, 2));
    string_replace(format, "DD", to_str(date.day, 2));
    string_replace(format, "MM", to_str(date.month, 2));
    string_replace(format, "MONTH", MONTHS[date.month-1]);
    string_replace(format, "MON", MONTHS_ABBR[date.month-1]);
    string_replace(format, "YEAR", to_str(date.year, 4));
    string_replace(format, "YYYY", to_str(date.year, 4));
    string_replace(format, "YY", to_str(date.year % 100, 2));

    return format;
}

static std::regex create_regex(const std::string &format) {
    try {
        return std::regex(format, std::regex::icase);
    } catch (std::regex_error &) {
        throw layout_error(fmt::format("Espressione regolare non valida: {}", format));
    }
}

std::vector<std::string> search_regex_all(const std::string &format, const std::string &value, int index) {
    std::vector<std::string> ret;
    std::regex expression = create_regex(format);
    std::transform(
        std::sregex_iterator(value.begin(), value.end(), expression),
        std::sregex_iterator(),
        std::inserter(ret, ret.begin()),
        [index](const auto &match) { return match.str(index); });
    return ret;
}

std::string search_regex(const std::string &format, const std::string &value, int index) {
    std::regex expression = create_regex(format);
    std::smatch match;
    if (std::regex_search(value, match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}

std::string string_replace_regex(const std::string &format, const std::string &value, const std::string &str) {
    return std::regex_replace(value, create_regex(format), str);
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