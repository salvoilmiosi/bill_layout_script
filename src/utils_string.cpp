#include "utils.h"

#include <algorithm>
#include <iostream>
#include <charconv>
#include <regex>

#include <fmt/format.h>

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

int cstoi(std::string_view str) {
    int ret;
    auto result = std::from_chars(str.begin(), str.end(), ret);
    if (result.ec == std::errc::invalid_argument) {
        throw std::invalid_argument(std::string(str));
    }
    return ret;
}

size_t string_findicase(std::string_view str, std::string_view str2, size_t index) {
    return std::distance(str.begin(), std::search(
        str.begin() + index, str.end(),
        str2.begin(), str2.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    ));
}

std::string string_format(std::string_view str, const std::vector<std::string> &fmt_args) {
    static constexpr char FORMAT_CHAR = '$';
    std::string ret;
    auto it = str.begin();
    while (it != str.end()) {
        if (*it == FORMAT_CHAR) {
            ++it;
            if (it == str.end()) {
                ret += FORMAT_CHAR;
                break;
            }
            if (*it >= '0' && *it <= '9') {
                size_t idx = 0;
                while (*it >= '0' && *it <= '9') {
                    idx = idx * 10 + (*it - '0');
                    ++it;
                }
                if (idx < fmt_args.size()) {
                    ret += fmt_args[idx];
                    continue;
                } else {
                    throw std::runtime_error(fmt::format("Stringa di formato non valida: {}", str));
                }
            } else {
                ret += FORMAT_CHAR;
                if (*it != FORMAT_CHAR) {
                    ret += *it;
                }
            }
        } else {
            ret += *it;
        }
        ++it;
    }
    return ret;
}

static std::regex create_regex(std::string_view format) {
    try {
        return std::regex(format.begin(), format.end(), std::regex::icase);
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", format, error.what()));
    }
}

std::vector<std::string> search_regex_all(std::string_view format, std::string_view value, int index) {
    std::vector<std::string> ret;
    std::regex expression = create_regex(format);
    std::transform(
        std::cregex_iterator(value.begin(), value.end(), expression),
        std::cregex_iterator(),
        std::back_inserter(ret),
        [index](const auto &match) { return match.str(index); });
    return ret;
}

std::vector<std::string> search_regex_captures(std::string_view format, std::string_view value) {
    std::vector<std::string> ret;
    std::regex expression = create_regex(format);
    std::cmatch match;
    if (std::regex_search(value.begin(), value.end(), match, expression)) {
        for (size_t i=1; i<match.size(); ++i) {
            ret.push_back(match.str(i));
        }
    }
    return ret;
}

std::string search_regex(std::string_view format, std::string_view value, int index) {
    std::regex expression = create_regex(format);
    std::cmatch match;
    if (std::regex_search(value.begin(), value.end(), match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}

std::string &string_replace_regex(std::string &value, std::string_view format, const std::string &str) {
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