#include "functions.h"
#include "utils.h"

#include <regex>
#include <numeric>

#include <fmt/format.h>

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
template<std::ranges::input_range R>
static std::string string_format(std::string_view str, R &&fmt_args) {
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

static std::regex create_regex(std::string regex) {
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

        auto number_regex = [&](){
            std::string tho = char_to_regex_str(intl::thousand_sep());
            std::string dec = char_to_regex_str(intl::decimal_point());
            
            std::string ret = "-?(?:";
            if (!tho.empty()) {
                ret += "\\d{1,3}(?:" + tho + "\\d{3})*"
                "(?:" + dec + "\\d+)?|";
            }
            ret += "\\d+(?:" + dec + "\\d+)?)(?!\\d)";
            return ret;
        };

        string_replace(regex, "\\N", number_regex());
        return std::regex(regex, std::regex::icase);
    } catch (const std::regex_error &error) {
        throw std::runtime_error(fmt::format("Espressione regolare non valida: {0}\n{1}", regex, error.what()));
    }
}

// Cerca la posizione di str2 in str senza fare differenza tra maiuscole e minuscole
static size_t string_find_icase(std::string_view str, std::string_view str2, size_t index) {
    return std::ranges::search(
        str.substr(std::min(str.size(), index)),
        str2, {}, toupper, toupper
    ).begin() - str.begin();
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
        UNIT_SEPARATOR);
}

// cerca la regex in str e ritorna i valori trovati
static std::string search_regex_matches(const std::string &regex, std::string_view value, int index) {
    auto reg = create_regex(regex);
    return string_join(
        std::ranges::subrange(
            std::cregex_token_iterator(value.begin(), value.end(), reg, index),
            std::cregex_token_iterator()),
        UNIT_SEPARATOR);
}

// restituisce un'espressione regolare che parsa una riga di una tabella
template<std::ranges::input_range R>
static std::string table_row_regex(std::string_view header, R &&names) {
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

// Viene creata un'espressione regolare che corrisponde alla stringa di formato valido per strptime,
// poi cerca la data in value e la parsa. Ritorna time_t=0 se c'e' errore.
static wxDateTime search_date(std::string_view value, const std::string &format, std::string regex, int index) {
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

    wxDateTime date(time_t(0));
    wxString::const_iterator end;
    date.ParseFormat(search_regex(regex, value, index).str(), format, wxDateTime(time_t(0)), &end);
    return date;
}

const function_map function_lookup = {
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
    {"abs", [](fixed_point a) { return a.abs(); }},
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
        for (const auto &s : string_split(str, UNIT_SEPARATOR)) {
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
        return search_date(str, format, regex.value_or(""), index.value_or(1));
    }},
    {"month", [](std::string_view str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return search_date(str, format, regex.value_or(""), index.value_or(1)).SetDay(1);
    }},
    {"replace", [](std::string &&str, std::string_view from, std::string_view to) {
        return string_replace(str, from, to);
    }},
    {"date_format", [](wxDateTime date, const std::string &format) {
        return date.Format(format).ToStdString();
    }},
    {"month_add", [](wxDateTime date, int num) {
        return date.Add(wxDateSpan(0, num));
    }},
    {"last_day", [](wxDateTime date) {
        return date.SetToLastMonthDay(date.GetMonth(), date.GetYear());
    }},
    {"date_between", [](wxDateTime date, wxDateTime date_begin, wxDateTime date_end) {
        return date.IsBetween(date_begin, date_end);
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
        return variable(std::string(str.substr(std::min(int(str.size()), pos), count.value_or(std::string_view::npos))));
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