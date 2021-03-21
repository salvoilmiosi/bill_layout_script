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
        return variable();
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

// Aggiunge slash nei caratteri da escapare per regex
static std::string escape_regex_char(char c) {
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

// Genera la regex per parsare numeri secondo il locale selezionato
static std::string number_regex() {
    auto tho = intl::thousand_sep();
    auto dec = intl::decimal_point();
    if (tho) {
        return fmt::format("(?:-?(?:\\d{{1,3}}(?:{0}\\d{{3}})*|\\d+)(?:{1}\\d+)?(?!\\d))", escape_regex_char(tho), escape_regex_char(dec));
    } else {
        return fmt::format("(?:-?\\d+(?:{0}\\d+)?(?!\\d))", escape_regex_char(dec));
    }
};

// Costruisce un oggetto std::regex
static std::regex create_regex(std::string regex) {
    try {
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

// converte solo i primi caratteri di una stringa in maiuscolo e il resto in minuscolo
static std::string &string_capitalize(std::string &str) {
    bool cap = true;
    for (auto &ch : str) {
        if (isspace(ch) || ch == '.') {
            cap = true;
        } else if (isalpha(ch) && cap) {
            cap = false;
            ch = toupper(ch);
        } else {
            ch = tolower(ch);
        }
    }
    return str;
}

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
static variable search_regex(const std::string &regex, std::string_view value, size_t index) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return variable();
    return match.str(index);
}

// cerca la regex in str e ritorna tutti i capture del primo valore trovato
static variable search_regex_captures(const std::string &regex, std::string_view value) {
    std::cmatch match;
    if (!std::regex_search(value.begin(), value.end(), match, create_regex(regex))) return variable();
    return string_join(
        std::ranges::subrange(match.begin() + 1, match.end())
        | std::views::transform([](const auto &m) {
            return std::string_view(m.first, m.second);
        }),
        UNIT_SEPARATOR);
}

// cerca la regex in str e ritorna i valori trovati
static std::string search_regex_matches(const std::string &regex, std::string_view value, size_t index) {
    auto reg = create_regex(regex);
    return string_join(
        std::ranges::subrange(
            std::cregex_token_iterator(value.begin(), value.end(), reg, index),
            std::cregex_token_iterator()),
        UNIT_SEPARATOR);
}

template<std::ranges::input_range R>
static std::string table_header(std::string_view header, R &&names) {
    return string_join(names
        | std::views::transform([&, pos = header.begin()](std::string_view name) mutable {
            std::cmatch match;
            if (std::regex_search(pos, header.end(), match, std::regex(name.begin(), name.end(), std::regex::icase))) {
                auto match_begin = match[0].first;
                auto match_end = match[0].second;
                pos = std::find_if_not(match_end, header.end(), isspace);
                if (pos == header.end()) {
                    return fmt::format("{}:-1", match_begin - header.begin());
                } else {
                    return fmt::format("{}:{}", match_begin - header.begin(), pos - match_begin);
                }
            } else {
                return std::string();
            }
        }),
    UNIT_SEPARATOR);
}

static std::string table_row(std::string_view row, std::string_view indices) {
    return string_join(string_split(indices, UNIT_SEPARATOR)
        | std::views::transform([&](std::string_view str) {
            std::cmatch match;
            if (std::regex_match(str.begin(), str.end(), match, std::regex("(\\d+):(-?\\d+)"))) {
                return row.substr(string_toint(match.str(1)), string_toint(match.str(2)));
            }
            return std::string_view();
        }),
    UNIT_SEPARATOR);
}

// Prende un formato per strptime e lo converte in una regex corrispondente
static std::string date_regex(std::string_view format) {
    std::string ret = "\\b";
    for (auto it = format.begin(); it != format.end(); ++it) {
        if (*it == '%') {
            ++it;
            switch (*it) {
            case 'h':
            case 'b':
            case 'B':
                ret += "\\w+";
                break;
            case 'd':
                ret += "\\d{1,2}";
                break;
            case 'm':
            case 'y':
                ret += "\\d{2}";
                break;
            case 'Y':
                ret += "\\d{4}";
                break;
            case 'n':
                ret += '\n';
                break;
            case 't':
                ret += '\t';
                break;
            case '%':
                ret += '%';
                break;
            default:
                throw std::invalid_argument("Stringa formato data non valida");
            }
        } else {
            ret += escape_regex_char(*it);
        }
    }
    ret += "\\b";
    return ret;
}

// Viene creata un'espressione regolare che corrisponde alla stringa di formato valido per strptime,
// poi cerca la data in value e la parsa. Ritorna time_t=0 se c'e' errore.
static variable search_date(std::string_view value, const std::string &format, std::string regex, size_t index) {
    if (regex.empty()) {
        regex = date_regex(format);
        index = 0;
    } else {
        string_replace(regex, "\\D", date_regex(format));
    }

    if (auto search_res = search_regex(regex, value, index); !search_res.is_null()) {
        wxDateTime date;
        wxString::const_iterator end;
        if (date.ParseFormat(search_res.as_string(), format, wxDateTime(time_t(0)), &end)) {
            return date;
        }
    }
    return variable();
}

const function_map function_lookup {
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
    {"null", []{ return variable(); }},
    {"num", [](const variable &var) {
        if (var.is_number()) return var;
        return parse_num(var.as_view());
    }},
    {"aggregate", [](std::string_view str) {
        variable ret;
        for (const auto &s : string_split(str, UNIT_SEPARATOR)) {
            ret.append(parse_num(s));
        }
        return ret;
    }},
    {"lines", [](std::string &&str) {
        std::ranges::replace(str, '\n', UNIT_SEPARATOR);
        return str;
    }},
    {"list", [](varargs<std::string_view> args) {
        return string_join(args, UNIT_SEPARATOR);
    }},
    {"sum", [](varargs<fixed_point> args) {
        return std::accumulate(args.begin(), args.end(), fixed_point());
    }},
    {"trunc", [](fixed_point num, int decimal_places) {
        int pow = dec::dec_utils<dec::def_round_policy>::pow10(decimal_places);
        return fixed_point((num * pow).getAsInteger()) / pow;
    }},
    {"max", [](varargs<variable, 1> args) {
        return *std::ranges::max_element(args);
    }},
    {"min", [](varargs<variable, 1> args) {
        return *std::ranges::min_element(args);
    }},
    {"percent", [](const std::string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable();
        }
    }},
    {"table_header", [](std::string_view header, varargs<std::string_view> names) {
        return table_header(header, names);
    }},
    {"table_row", [](std::string_view row, std::string_view indices) {
        return table_row(row, indices);
    }},
    {"search", [](std::string_view str, const std::string &regex, optional_size<1> index) {
        return search_regex(regex, str, index);
    }},
    {"matches", [](std::string_view str, const std::string &regex, optional_size<1> index) {
        return search_regex_matches(regex, str, index);
    }},
    {"captures", [](std::string_view str, const std::string &regex) {
        return search_regex_captures(regex, str);
    }},
    {"replace", [](std::string &&str, std::string_view from, std::string_view to) {
        return string_replace(str, from, to);
    }},
    {"date_regex", [](std::string_view format) {
        return date_regex(format);
    }},
    {"date", [](std::string_view str, const std::string &format, optional<std::string> regex, optional_size<1> index) {
        return search_date(str, format, regex, index);
    }},
    {"month", [](std::string_view str, const std::string &format, optional<std::string> regex, optional_size<1> index) {
        if (auto date = search_date(str, format, regex, index); !date.is_null()) {
            return variable(date.as_date().SetDay(1));
        }
        return variable();
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
    {"capitalize", [](std::string &&str) {
        return string_capitalize(str);
    }},
    {"if", [](bool condition, const variable &var_if, optional<variable> var_else) {
        return condition ? var_if : var_else;
    }},
    {"ifnot", [](bool condition, const variable &var_if, optional<variable> var_else) {
        return condition ? var_else : var_if;
    }},
    {"trim", [](std::string_view str) {
        return string_trim(str);
    }},
    {"lpad", [](std::string_view str, int amount) {
        return fmt::format("{0: >{1}}", str, str.size() + amount);
    }},
    {"rpad", [](std::string_view str, int amount) {
        return fmt::format("{0: <{1}}", str, str.size() + amount);
    }},
    {"contains", [](std::string_view str, std::string_view str2) {
        return string_find_icase(str, str2, 0) < str.size();
    }},
    {"substr", [](std::string_view str, size_t pos, optional_size<std::string_view::npos> count) {
        return std::string(str.substr(std::min(str.size(), pos), count));
    }},
    {"strcat", [](varargs<std::string_view> args) {
        return string_join(args);
    }},
    {"strlen", [](std::string_view str) {
        return str.size();
    }},
    {"indexof", [](std::string_view str, std::string_view value, optional<size_t> index) {
        return string_find_icase(str, value, index);
    }},
    {"tolower", [](std::string_view str) {
        auto view = str | std::views::transform(tolower);
        return std::string{view.begin(), view.end()};
    }},
    {"toupper", [](std::string_view str) {
        auto view = str | std::views::transform(toupper);
        return std::string{view.begin(), view.end()};
    }},
    {"isempty", [](std::string_view str) {
        return str.empty();
    }},
    {"format", [](std::string_view format, varargs<std::string_view> args) {
        return string_format(format, args);
    }},
    {"coalesce", [](varargs<variable> args) {
        for (const auto &arg : args) {
            if (!arg.is_null()) return arg;
        }
        return variable();
    }},
};