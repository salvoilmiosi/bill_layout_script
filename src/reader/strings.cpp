#include "functions.h"

#include "utils.h"
#include "layout.h"

#include <regex>
#include <algorithm>
#include <fmt/format.h>
#include <wx/intl.h>

std::string parse_number(const std::string &str) {
    static char decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
    std::string out;
    for (char c : str) {
        if (std::isdigit(c) || c == '-') {
            out += c;
        } else if (c == decimal_point) {
            out += '.';
        }
    }
    return out;
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

std::regex create_regex(const std::string &format) {
    try {
        return std::regex(format, std::regex::icase);
    } catch (const std::regex_error &error) {
        throw layout_error(fmt::format("Espressione regolare non valida: {0}\n{1}", format, error.what()));
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