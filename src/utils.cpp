#include "utils.h"

#include <algorithm>
#include <iostream>
#include <regex>

#include <fmt/format.h>
#include <wx/datetime.h>

#include "intl.h"

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

std::string string_join(const std::vector<std::string> &vec, std::string_view separator) {
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
    std::transform(str.begin(), str.end(), str.begin(), [](auto ch) {
        return std::tolower(ch);
    });
    return str;
}

std::string string_toupper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](auto ch) {
        return std::toupper(ch);
    });
    return str;
}

void string_trim(std::string &str) {
#ifdef _WIN32
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
#endif
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](auto ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](auto ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

int string_replace(std::string &str, std::string_view from, std::string_view to) {
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

std::string read_all(std::istream &stream) {
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

int cstoi(std::string_view str, int base) {
    int mul = 1;
    if (str.starts_with('-')) {
        mul = -mul;
        str.remove_prefix(1);
    }

    if (str.empty()) {
        throw std::invalid_argument(std::string(str));
    }

    int ret = 0;
    for (auto it = str.rbegin(); it != str.rend(); ++it, mul *= base) {
        if (*it >='0' && *it < '0' + base) {
            ret += (*it - '0') * mul;
        } else {
            throw std::invalid_argument(std::string(str));
        }
    }
    return ret;
}

double cstof(std::string_view str, int base) {
    double mul = 1.0;
    if (str.starts_with('-')) {
        mul = -mul;
        str.remove_prefix(1);
    }

    if (str.empty() || str == ".") {
        throw std::invalid_argument(std::string(str));
    }

    double ret = 0.0;
    double lmul = mul;
    
    auto dec_point = str.find('.');
    for (auto it = str.rend() - dec_point; it != str.rend(); ++it, lmul *= base) {
        if (*it >='0' && *it < '0' + base) {
            ret += (*it - '0') * lmul;
        } else {
            throw std::invalid_argument(std::string(str));
        }
    }

    if (dec_point != std::string_view::npos) {
        for (auto it = str.begin() + dec_point + 1; it != str.end(); ++it) {
            mul /= base;
            if (*it >='0' && *it < '0' + base) {
                ret += (*it - '0') * mul;
            } else {
                throw std::invalid_argument(std::string(str));
            }
        }
    }
    return ret;
}

static wxDateTime string_to_date(std::string_view str) {
    static std::regex expression("(\\d{4})-(\\d{2})(-(\\d{2}))?");

    std::cmatch match;
    if (std::regex_match(str.begin(), str.end(), match, expression)) {
        int year = cstoi(match.str(1));
        int month = cstoi(match.str(2));
        int day = 1;
        auto day_str = match.str(4);
        if (!day_str.empty()) {
            day = cstoi(day_str);
        }
        if (month <= 0 || month > 12) {
            throw std::invalid_argument("Formato data non valido");
        }
        if (day <= 0 || day > wxDateTime::GetNumberOfDays(static_cast<wxDateTime::Month>(month - 1), year)) {
            throw std::invalid_argument("Formato data non valido");
        }
        return wxDateTime(day, static_cast<wxDateTime::Month>(month - 1), year);
    } else {
        throw std::invalid_argument("Formato data non valido");
    }
}

inline bool search_date(wxDateTime &dt, const wxString &format, std::string_view value, const std::string &regex, int index) {
    wxString::const_iterator end;
    return dt.ParseFormat(search_regex(regex, value, index), format, wxDateTime(time_t(0)), &end);
}

std::string parse_date(const std::string &format, std::string_view value, const std::string &regex, int index) {
    wxDateTime dt;
    if(!search_date(dt, format, value, regex, index)) {
        return "";
    }

    return dt.Format("%Y-%m-%d").ToStdString();
}

std::string parse_month(const std::string &format, std::string_view value, const std::string &regex, int index) {
    wxDateTime dt;
    if (format.empty()) {
        try {
            dt = string_to_date(value);
        } catch (const std::invalid_argument &) {
            return "";
        }
    } else {
        if (!search_date(dt, format, value, regex, index)) {
            return "";
        }
    }
    return dt.Format("%Y-%m").ToStdString();
}

std::string date_month_add(std::string_view str, int num) {
    try {
        wxDateTime dt = string_to_date(str);
        dt += wxDateSpan(0, num);

        return dt.Format("%Y-%m").ToStdString();
    } catch (const std::invalid_argument &) {
        return "";
    }
}

std::string date_format(std::string_view str, const std::string &format) {
    try {
        wxDateTime dt = string_to_date(str);

        return dt.Format(format).ToStdString();
    } catch (const std::invalid_argument &) {
        return "";
    }
}

size_t string_findicase(std::string_view str, std::string_view str2, size_t index) {
    return std::distance(str.begin(), std::search(
        str.begin() + index, str.end(),
        str2.begin(), str2.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    ));
}

std::string string_format(std::string str, const std::vector<std::string> &fmt_args) {
    for (size_t i=0; i<fmt_args.size(); ++i) {
        string_replace(str, fmt::format("${}", i), fmt_args[i]);
    }
    return str;
}

std::regex create_regex(std::string format) {
    using namespace std::string_literals;

    string_replace(format, "%N", intl::number_format());

    try {
        return std::regex(format, std::regex::icase);
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", format, error.what()));
    }
}

std::vector<std::string> search_regex_all(const std::string &format, std::string_view value, int index) {
    std::vector<std::string> ret;
    std::regex expression = create_regex(format);
    std::transform(
        std::cregex_iterator(value.begin(), value.end(), expression),
        std::cregex_iterator(),
        std::back_inserter(ret),
        [index](const auto &match) { return match.str(index); });
    return ret;
}

std::string search_regex(const std::string &format, std::string_view value, int index) {
    std::regex expression = create_regex(format);
    std::cmatch match;
    if (std::regex_search(value.begin(), value.end(), match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}

std::string &string_replace_regex(std::string &value, const std::string &format, const std::string &str) {
    return value = std::regex_replace(value, create_regex(format), str);
}

std::string singleline(std::string input) {
    size_t index = 0;
    while (true) {
        index = input.find_first_of("\t\r\n\v\f", index);
        if (index == std::string::npos) break;

        input.replace(index, 1, " ");
        ++index;
    }
    return input;
}