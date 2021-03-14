#include "utils.h"

#include <regex>

#include <fmt/format.h>

void string_replace(std::string &str, std::string_view from, std::string_view to) {
    size_t index = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, from.size(), to);
        index += to.size();
    }
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

std::string search_regex(const std::string &regex, std::string_view value, int index) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return "";
    return match.str(index);
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

std::string search_regex_matches(const std::string &regex, std::string_view value, int index) {
    return string_join(
        std::ranges::subrange(std::cregex_iterator(value.begin(), value.end(), create_regex(regex)), std::cregex_iterator())
        | std::views::transform([&](auto &match) {
            return match.str(index);
        }),
        RESULT_SEPARATOR);
}

std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names) {
    std::string ret;
    size_t begin = 0;
    size_t len = 0;
    for (const auto &name : names) {
        if (header.size() < begin + len) break;
        size_t i = string_find_icase(header, name, begin + len);
        len = name.size();
        ret += fmt::format("(.{{{}}})", i - begin);
        begin = i;
    }
    ret += "(.+)";
    return ret;
}

static bool search_date(wxDateTime &dt, std::string_view value, const std::string &format, std::string regex, int index) {
    std::string date_regex = "\\b";
    for (auto it = format.begin(); it != format.end(); ++it) {
        if (*it == '.') {
            date_regex += "\\.";
        } else if (*it == '%') {
            ++it;
            switch (*it) {
            case 'h':
            case 'b':
            case 'B':
                date_regex += "\\w+";
                break;
            case 'd':
                date_regex += "\\d{1,2}";
                break;
            case 'm':
            case 'y':
                date_regex += "\\d{2}";
                break;
            case 'Y':
                date_regex += "\\d{4}";
                break;
            case 'n':
                date_regex += '\n';
                break;
            case 't':
                date_regex += '\t';
                break;
            case '%':
                date_regex += '%';
                break;
            default:
                throw std::invalid_argument("Stringa formato data non valida");
            }
        } else {
            date_regex += *it;
        }
    }
    date_regex += "\\b";

    if (regex.empty()) {
        regex = "(" + date_regex +  ")";
    } else {
        string_replace(regex, "\\D", date_regex);
    }

    wxString::const_iterator end;
    return dt.ParseFormat(search_regex(regex, value, index), format, wxDateTime(time_t(0)), &end);
}

time_t parse_date(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        return dt.GetTicks();
    }
    return 0;
}

time_t parse_month(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        dt.SetDay(1);
        return dt.GetTicks();
    }
    return 0;
}