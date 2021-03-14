#include "utils.h"

#include <algorithm>
#include <iostream>
#include <charconv>
#include <regex>

#include <fmt/format.h>

#include "intl.h"

std::string string_tolower(std::string_view str) {
    auto view = str | std::views::transform(tolower);
    return {view.begin(), view.end()};
}

std::string string_toupper(std::string_view str) {
    auto view = str | std::views::transform(toupper);
    return {view.begin(), view.end()};
}

std::string string_trim(std::string_view str) {
    auto view = str
        | std::views::drop_while(isspace)
        | std::views::reverse
        | std::views::drop_while(isspace)
        | std::views::reverse;
    return {view.begin(), view.end()};
}

std::string string_singleline(std::string_view str) {
    std::string ret;
    std::ranges::unique_copy(str | std::views::transform([](auto ch) {
        return isspace(ch) ? ' ' : ch;
    }), std::back_inserter(ret), [](auto a, auto b) {
        return a == ' ' && b == ' ';
    });
    return ret;
}

void string_replace(std::string &str, std::string_view from, std::string_view to) {
    size_t index = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, from.size(), to);
        index += to.size();
    }
}

std::string read_all(std::istream &stream) {
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

template<typename T>
inline T cston(std::string_view str) {
    T ret;
    auto result = std::from_chars(str.begin(), str.end(), ret);
    if (result.ec == std::errc::invalid_argument) {
        throw std::invalid_argument(std::string(str));
    }
    return ret;
}

int cstoi(std::string_view str) {
    return cston<int>(str);
}

#ifdef CHARCONV_FLOAT
float cstof(std::string_view str) {
    return cston<float>(str);
}

double cstod(std::string_view str) {
    return cston<double>(str);
}
#endif

size_t string_findicase(std::string_view str, std::string_view str2, size_t index) {
    return std::distance(str.begin(), std::ranges::search(str.substr(index), str2, [](char a, char b) {
        return toupper(a) == toupper(b);
    }).begin());
}

std::string string_format(std::string_view str, const varargs<std::string_view> &fmt_args) {
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
                    auto view = fmt_args[idx];
                    ret.append(view.begin(), view.end());
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

static const std::regex &create_regex(std::string regex) {
    static std::unordered_map<std::string, std::regex> compiled_regexes;
    auto [lower, upper] = compiled_regexes.equal_range(regex);
    if (lower != upper) {
        return lower->second;
    }

    try {
        auto char_to_regex_str = [](char c) -> std::string {
            switch (c) {
            case '.':
            case '+':
            case '*':
            case '?':
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                return std::string("\\") + c;
            case '\0':  return "";
            default:    return std::string(&c, 1);
            }
        };

        string_replace(regex, "\\N", "-?(?:\\d{1,3}(?:"
            + char_to_regex_str(intl::thousand_sep()) + "\\d{3})*(?:"
            + char_to_regex_str(intl::decimal_point()) + "\\d+)?|\\d+(?:"
            + char_to_regex_str(intl::decimal_point()) + "\\d+)?)(?!\\d)");
        return compiled_regexes.emplace_hint(lower, regex, std::regex(regex, std::regex::icase))->second;
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", regex, error.what()));
    }
}

std::string search_regex_all(const std::string &regex, std::string_view value, int index) {
    return string_join(
        std::ranges::subrange(std::cregex_iterator(value.begin(), value.end(), create_regex(regex)), std::cregex_iterator())
        | std::views::transform([&](auto &match) {
            return match.str(index);
        }),
        RESULT_SEPARATOR);
}

std::string search_regex_captures(const std::string &regex, std::string_view value) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return "";
    return string_join(
        std::views::iota(1, int(match.size()))
        | std::views::transform([&](int index) {
            return match.str(index);
        }),
        RESULT_SEPARATOR);
}

std::string search_regex(const std::string &regex, std::string_view value, int index) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return "";
    return match.str(index);
}

std::string string_replace_regex(const std::string &value, const std::string &regex, const std::string &str) {
    return std::regex_replace(value, create_regex(regex), str);
}

std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names) {
    std::string ret;
    size_t begin = 0;
    size_t len = 0;
    for (const auto &name : names) {
        if (header.size() < begin + len) break;
        size_t i = string_findicase(header, name, begin + len);
        len = name.size();
        ret += fmt::format("(.{{{}}})", i - begin);
        begin = i;
    }
    ret += "(.+)";
    return ret;
}