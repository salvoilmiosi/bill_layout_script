#include "utils.h"

#include <algorithm>
#include <iostream>
#include <charconv>
#include <regex>

#include <fmt/format.h>

#include "intl.h"

std::vector<std::string_view> string_split(std::string_view str, char separator) {
    std::vector<std::string_view> ret;

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

std::string string_split_n(std::string_view str, int nparts) {
    std::string ret;
    float len = float(str.size()) / nparts;
    float loc = 0;
    for (int i=0; i<nparts; ++i) {
        if (i != 0) {
            ret += RESULT_SEPARATOR;
        }
        ret += str.substr(std::round(loc), std::round(loc + len) - std::round(loc));
        loc += len;
    }
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
    for (char &c : str) {
        c = std::tolower(c);
    };
    return str;
}

std::string string_toupper(std::string str) {
    for (char &c : str) {
        c = std::toupper(c);
    };
    return str;
}

void string_trim(std::string &str) {
#ifdef _WIN32
    std::erase(str, '\r');
#endif
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](auto ch) {
        return !std::isspace(ch);
    }));
    str.erase(std::find_if(str.rbegin(), str.rend(), [](auto ch) {
        return !std::isspace(ch);
    }).base(), str.end());
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
    return std::distance(str.begin(), std::search(
        str.begin() + index, str.end(),
        str2.begin(), str2.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    ));
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
            + char_to_regex_str(intl::decimal_point()) + "\\d+)?|\\d+)(?!\\d)");
        return compiled_regexes.emplace_hint(lower, regex, std::regex(regex, std::regex::icase))->second;
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", regex, error.what()));
    }
}

std::string search_regex_all(const std::string &regex, std::string_view value, int index) {
    std::string ret;
    std::regex expression = create_regex(regex);
    auto it = std::cregex_iterator(value.begin(), value.end(), expression);
    bool first = true;
    for (; it != std::cregex_iterator(); ++it) {
        if (!first) {
            ret += RESULT_SEPARATOR;
        }
        first = false;
        ret += it->str(index);
    }
    return ret;
}

std::string search_regex_captures(const std::string &regex, std::string_view value) {
    std::string ret;
    std::regex expression = create_regex(regex);
    std::cmatch match;
    if (std::regex_search(value.begin(), value.end(), match, expression)) {
        for (size_t i=1; i<match.size(); ++i) {
            if (i != 1) {
                ret += RESULT_SEPARATOR;
            }
            ret += match.str(i);
        }
    }
    return ret;
}

std::string search_regex(const std::string &regex, std::string_view value, int index) {
    std::regex expression = create_regex(regex);
    std::cmatch match;
    if (std::regex_search(value.begin(), value.end(), match, expression)) {
        return match.str(index);
    } else {
        return "";
    }
}

std::string string_replace_regex(const std::string &value, const std::string &regex, const std::string &str) {
    return std::regex_replace(value, create_regex(regex), str);
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

std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names) {
    std::string ret;
    size_t begin = 0;
    size_t len = 0;
    for (const auto &name : names) {
        size_t i = string_findicase(header, name, begin + len);
        len = name.size();
        ret += fmt::format("(.{{{}}})", i - begin);
        begin = i;
    }
    ret += "(.+)";
    return ret;
}