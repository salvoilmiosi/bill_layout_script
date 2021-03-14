#include "functions.h"
#include "utils.h"

#include <regex>
#include <numeric>

#include <fmt/format.h>
#include <wx/datetime.h>

#include "intl.h"

// Converte una stringa in numero usando il formato del locale
static variable parse_num(std::string_view str) {
    fixed_point num;
    std::istringstream iss;
    iss.rdbuf()->pubsetbuf(const_cast<char *>(str.begin()), str.size());
    if (dec::fromStream(iss, dec::decimal_format(intl::decimal_point(), intl::thousand_sep()), num)) {
        return num;
    } else {
        return variable::null_var();
    }
};

// Formatta la stringa data, sostituendo $0 in fmt_args[0], $1 in fmt_args[1] e così via
static std::string string_format(std::string_view str, const varargs<std::string_view> &fmt_args) {
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

        string_replace(regex, " ", "\\s+");
        string_replace(regex, "\\N", "-?(?:\\d{1,3}(?:"
            + char_to_regex_str(intl::thousand_sep()) + "\\d{3})*(?:"
            + char_to_regex_str(intl::decimal_point()) + "\\d+)?|\\d+(?:"
            + char_to_regex_str(intl::decimal_point()) + "\\d+)?)(?!\\d)");
        return compiled_regexes.emplace_hint(lower, regex, std::regex(regex, std::regex::icase))->second;
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", regex, error.what()));
    }
}

// Cerca la posizione di str2 in str senza fare differenza tra maiuscole e minuscole
static size_t string_find_icase(std::string_view str, std::string_view str2, size_t index) {
    return std::distance(str.begin(), std::ranges::search(str.substr(index), str2, [](char a, char b) {
        return toupper(a) == toupper(b);
    }).begin());
}

// converte ogni carattere di spazio in " " e elimina gli spazi ripetuti
static std::string string_singleline(std::string_view str) {
    std::string ret;
    std::ranges::unique_copy(str | std::views::transform([](auto ch) {
        return isspace(ch) ? ' ' : ch;
    }), std::back_inserter(ret), [](auto a, auto b) {
        return a == ' ' && b == ' ';
    });
    return ret;
}

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
static variable search_regex(const std::string &regex, std::string_view value, int index) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return variable::null_var();
    return match.str(index);
}

// cerca la regex in str e ritorna tutti i capture del primo valore trovato
static variable search_regex_captures(const std::string &regex, std::string_view value) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return variable::null_var();
    return string_join(
        std::views::iota(1, int(match.size()))
        | std::views::transform([&](int index) {
            return match.str(index);
        }),
        RESULT_SEPARATOR);
}

// cerca la regex in str e ritorna i valori trovati
static std::string search_regex_matches(const std::string &regex, std::string_view value, int index) {
    return string_join(
        std::ranges::subrange(std::cregex_iterator(value.begin(), value.end(), create_regex(regex)), std::cregex_iterator())
        | std::views::transform([&](auto &match) {
            return match.str(index);
        }),
        RESULT_SEPARATOR);
}

// restituisce un'espressione regolare che parsa una riga di una tabella
static std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names) {
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
    return dt.ParseFormat(search_regex(regex, value, index).str(), format, wxDateTime(time_t(0)), &end);
}

// cerca una data
static time_t parse_date(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        return dt.GetTicks();
    }
    return 0;
}

// cerca una data e setta il giorno a 1
static time_t parse_month(std::string_view value, const std::string &format, const std::string &regex, int index) {
    wxDateTime dt;
    if (search_date(dt, value, format, regex, index)) {
        dt.SetDay(1);
        return dt.GetTicks();
    }
    return 0;
}

const std::map<std::string, function_handler, std::less<>> function_lookup {
    {"eq",  [](const variable &a, const variable &b) { return a == b; }},
    {"neq", [](const variable &a, const variable &b) { return a != b; }},
    {"lt",  [](const variable &a, const variable &b) { return a < b; }},
    {"gt",  [](const variable &a, const variable &b) { return a > b; }},
    {"leq", [](const variable &a, const variable &b) { return a <= b; }},
    {"geq", [](const variable &a, const variable &b) { return a >= b; }},
    {"int", [](int a) { return a; }},
    {"mod", [](int a, int b) { return a % b; }},
    {"add", [](fixed_point a, fixed_point b) { return a + b; }},
    {"sub", [](fixed_point a, fixed_point b) { return a - b; }},
    {"mul", [](fixed_point a, fixed_point b) { return a * b; }},
    {"div", [](fixed_point a, fixed_point b) { return a / b; }},
    {"neg", [](fixed_point a) { return -a; }},
    {"not", [](bool a) { return !a; }},
    {"and", [](bool a, bool b) { return a && b; }},
    {"or",  [](bool a, bool b) { return a || b; }},
    {"null", []{ return variable::null_var(); }},
    {"num", [](const variable &var) {
        if (var.empty() || var.type() == VAR_NUMBER) return var;
        return parse_num(var.str_view());
    }},
    {"aggregate", [](std::string_view str) {
        variable ret;
        for (const auto &s : string_split(str, RESULT_SEPARATOR)) {
            ret += parse_num(s);
        }
        return ret;
    }},
    {"sum", [](varargs<fixed_point> args) {
        return std::accumulate(args.begin(), args.end(), fixed_point());
    }},
    {"trunc", [](fixed_point num, int decimal_places) {
        int pow = dec::dec_utils<dec::def_round_policy>::pow10(decimal_places);
        return fixed_point((num * pow).getAsInteger()) / pow;
    }},
    {"max", [](varargs<variable> args) {
        if (args.empty()) {
            return variable::null_var();
        }
        return *std::ranges::max_element(args);
    }},
    {"min", [](varargs<variable> args) {
        if (args.empty()) {
            return variable::null_var();
        }
        return *std::ranges::min_element(args);
    }},
    {"percent", [](const std::string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable::null_var();
        }
    }},
    {"table_row_regex", [](std::string_view header, varargs<std::string_view> names) {
        return table_row_regex(header, names);
    }},
    {"search", [](std::string_view str, const std::string &regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }},
    {"matches", [](std::string_view str, const std::string &regex, std::optional<int> index) {
        return search_regex_matches(regex, str, index.value_or(1));
    }},
    {"captures", [](std::string_view str, const std::string &regex) {
        return search_regex_captures(regex, str);
    }},
    {"date", [](std::string_view str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_date(str, format, regex.value_or(""), index.value_or(1));
    }},
    {"month", [](std::string_view str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_month(str, format, regex.value_or(""), index.value_or(1));
    }},
    {"replace", [](std::string &&str, std::string_view from, std::string_view to) {
        return string_replace(str, from, to);
    }},
    {"date_format", [](time_t date, const std::string &format) {
        return wxDateTime(date).Format(format).ToStdString();
    }},
    {"month_add", [](time_t date, int num) {
        wxDateTime dt(date);
        dt += wxDateSpan(0, num);

        return dt.GetTicks();
    }},
    {"last_day", [](time_t date) {
        wxDateTime dt(date);
        dt.SetToLastMonthDay(dt.GetMonth(), dt.GetYear());
        return dt.GetTicks();
    }},
    {"date_between", [](time_t date, time_t date_begin, time_t date_end) {
        return wxDateTime(date).IsBetween(wxDateTime(date_begin), wxDateTime(date_end));
    }},
    {"singleline", [](std::string_view str) {
        return string_singleline(str);
    }},
    {"if", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }},
    {"ifnot", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_else.value_or(variable::null_var()) : var_if;
    }},
    {"trim", [](std::string_view str) {
        return string_trim(str);
    }},
    {"contains", [](std::string_view str, std::string_view str2) {
        return string_find_icase(str, str2, 0) < str.size();
    }},
    {"substr", [](std::string_view str, int pos, std::optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(std::string(str.substr(pos, count.value_or(std::string_view::npos))));
        }
        return variable::null_var();
    }},
    {"strcat", [](varargs<std::string_view> args) {
        return string_join(args);
    }},
    {"strlen", [](std::string_view str) {
        return str.size();
    }},
    {"indexof", [](std::string_view str, std::string_view value, std::optional<int> index) {
        return string_find_icase(str, value, index.value_or(0));
    }},
    {"tolower", [](std::string_view str) {
        auto view = str | std::views::transform(tolower);
        return std::string{view.begin(), view.end()};
    }},
    {"toupper", [](std::string_view str) {
        auto view = str | std::views::transform(toupper);
        return std::string{view.begin(), view.end()};
    }},
    {"isempty", [](const variable &var) {
        return var.empty();
    }},
    {"format", [](std::string_view format, varargs<std::string_view> args) {
        return string_format(format, args);
    }},
    {"coalesce", [](varargs<variable> args) {
        for (const auto &arg : args) {
            if (!arg.empty()) return arg;
        }
        return variable::null_var();
    }},
};