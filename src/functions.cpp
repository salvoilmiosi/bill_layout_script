#include "functions.h"

#include <regex>
#include <numeric>
#include <fstream>

#include <boost/locale.hpp>

#include "exceptions.h"
#include "utils.h"
#include "svstream.h"

namespace bls {

    // Converte una stringa in numero usando il formato del locale
    static variable parse_num(const std::locale &loc, std::string_view str) {
        fixed_point num;
        util::isviewstream iss{str};
        iss.imbue(loc);
        if (dec::fromStream(iss, num)) {
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
                        throw layout_error(fmt::format("Stringa di formato non valida: {}", str));
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
    static std::string number_regex(const std::locale &loc) {
        auto &facet = std::use_facet<std::numpunct<char>>(loc);
        auto grp = facet.grouping();
        std::string ret;
        if (grp.empty()) {
            ret = "\\d+";
        } else {
            auto tho = escape_regex_char(facet.thousands_sep());
            auto it = grp.rbegin();
            ret = fmt::format("\\d{{1,{0}}}(?:{1}\\d{{{0}}})*", int(*it), tho);
            for(++it; it != grp.rend(); ++it) {
                ret = fmt::format("(?:{2}{1}\\d{{{0}}}|\\d{{1,{0}}})", int(*it), tho, ret);
            }
            ret = fmt::format("(?:{}|\\d+)", ret);
        }
        return fmt::format("(?:-?{0}(?:{1}\\d+)?)(?!\\d)", ret, escape_regex_char(facet.decimal_point()));
    };

    // Costruisce un oggetto std::regex
    static std::regex create_regex(const std::locale &loc, std::string regex) {
        try {
            util::string_replace(regex, "\\N", number_regex(loc));
            return std::regex(regex, std::regex::icase);
        } catch (const std::regex_error &error) {
            throw layout_error(fmt::format("Espressione regolare non valida: {0}\n{1}", regex, error.what()));
        }
    }

    // Cerca la posizione di str2 in str senza fare differenza tra maiuscole e minuscole
    static auto string_find_icase(std::string_view str, std::string_view str2, size_t index) {
        return std::ranges::search(
            str.substr(std::min(str.size(), index)),
            str2, {}, toupper, toupper
        );
    }

    // converte ogni carattere di spazio in " " e elimina gli spazi ripetuti
    static std::string string_singleline(std::string_view str) {
        std::string ret;
        auto in_range = str | std::views::transform([](char ch) {
            return isspace(ch) ? ' ' : ch;
        });
        auto range = std::unique_copy(
            in_range.begin(), in_range.end(),
            std::back_inserter(ret),
        [](char a, char b) {
            return a == ' ' && b == ' ';
        });
        return ret;
    }

    inline auto match_to_view(const std::csub_match &m) {
        return std::string_view(m.first, m.second);
    };

    // cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
    static std::string_view search_regex(const std::locale &loc, std::string_view value, const std::string &regex, size_t index) {
        std::cmatch match;
        if (std::regex_search(value.data(), value.data() + value.size(), match, create_regex(loc, regex))) {
            if (index < match.size()) return match_to_view(match[index]);
        }
        return {value.end(), value.end()};
    }

    // cerca la regex in str e ritorna tutti i capture del primo valore trovato
    static std::vector<std::string> search_regex_captures(const std::locale &loc, std::string_view value, const std::string &regex) {
        std::cmatch match;
        if (!std::regex_search(value.data(), value.data() + value.size(), match, create_regex(loc, regex))) return {};
        return {match.begin() + 1, match.end()};
    }

    // cerca la regex in str e ritorna i valori trovati
    static std::vector<std::string> search_regex_all(const std::locale &loc, std::string_view value, const std::string &regex, size_t index) {
        auto reg = create_regex(loc, regex);
        auto view = std::ranges::subrange(
            std::cregex_token_iterator(value.data(), value.data() + value.size(), reg, index),
            std::cregex_token_iterator());
        return {view.begin(), view.end()};
    }

    template<std::ranges::input_range R>
    static std::vector<std::string> table_header(std::string_view value, R &&labels) {
        std::cmatch header_match;
        std::regex header_regex(fmt::format(".*{}.*", util::string_join(labels |
        std::views::transform([first=true](std::string_view str) mutable {
            if (first) {
                first = false;
                return std::string(str);
            } else {
                return fmt::format("(?:{})?", str);
            }
        }), ".*")), std::regex::icase);
        if (!std::regex_search(value.data(), value.data() + value.size(), header_match, header_regex)) {
            return {};
        }
        auto header_str = match_to_view(header_match[0]);
        auto view = labels | std::views::transform([&, pos = header_str.data()](std::string_view label) mutable {
            std::cmatch match;
            if (std::regex_search(&*pos, header_str.data() + header_str.size(), match, std::regex(label.begin(), label.end(), std::regex::icase))) {
                auto match_str = match_to_view(match[0]);
                pos = std::find_if_not(match_str.data() + match_str.size(), header_str.data() + header_str.size(), isspace);
                if (pos == header_str.data() + header_str.size()) {
                    return fmt::format("{}:-1", match_str.data() - header_str.data());
                } else {
                    return fmt::format("{}:{}", match_str.data() - header_str.data(), pos - match_str.data());
                }
            } else {
                return std::string();
            }
        });
        return {view.begin(), view.end()};
    }

    static std::vector<std::string> table_row(std::string_view row, const std::vector<std::string> &indices) {
        auto view = indices | std::views::transform([&](std::string_view str) {
            std::cmatch match;
            if (std::regex_match(str.data(), str.data() + str.size(), match, std::regex("(\\d+):(-?\\d+)"))) {
                size_t begin = util::string_to<int>(match.str(1));
                if (begin < row.size()) {
                    size_t len = util::string_to<int>(match.str(2));
                    return row.substr(begin, len);
                }
            }
            return std::string_view();
        });
        return {view.begin(), view.end()};
    }

    // Prende un formato per strptime e lo converte in una regex corrispondente
    static std::string date_regex(std::string_view format) {
        std::string ret = "\\b";
        for (auto it = format.begin(); it != format.end(); ++it) {
            if (*it == '%') {
                ++it;
                switch (*it) {
                case 'a': case 'A':
                case 'b': case 'B':
                case 'h':
                    ret += "[^\\s]+";
                    break;
                case 'w':
                case 'm':
                case 'y':
                case 'H':
                case 'l':
                case 'd':
                case 'M':
                case 'S':
                    ret += "\\d{1,2}";
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
    static datetime search_date(const std::locale &loc, std::string_view value, const std::string &format, std::string regex, size_t index) {
        if (regex.empty()) {
            regex = date_regex(format);
            index = 0;
        } else {
            util::string_replace(regex, "\\D", date_regex(format));
        }

        if (auto search_res = search_regex(loc, value, regex, index); !search_res.empty()) {
            return datetime::parse_date(loc, search_res, format);
        }
        return datetime();
    }

    static std::string_view string_between(const std::locale &loc, std::string_view str, const variable &from, const variable &to, bool outer_left, bool outer_right) {
        auto search_range = [&](const variable &expr) -> std::string_view {
            if (expr.is_regex()) {
                return search_regex(loc, str, expr.as_string(), 0);
            } else {
                auto range = string_find_icase(str, expr.as_view(), 0);
                return {range.begin(), range.end()};
            }
        };
        if (!from.is_null()) {
            auto from_span = search_range(from);
            str = std::string_view(outer_left ? from_span.data() : from_span.data() + from_span.size(), str.data() + str.size());
        }
        if (!to.is_null()) {
            auto to_span = search_range(to);
            str = std::string_view(str.data(), outer_right ? to_span.data() + to_span.size() : to_span.data());
        }
        return str;
    }

    const function_map function_lookup {
        {"str", [](const std::string &str) { return str; }},
        {"num", [](const std::locale &loc, const variable &var) {
            if (var.is_number()) return var;
            return parse_num(loc, var.as_view());
        }},
        {"int", [](int a) { return a; }},
        {"bool",[](bool a) { return a; }},
        {"eq",  [](const variable &a, const variable &b) { return a == b; }},
        {"neq", [](const variable &a, const variable &b) { return a != b; }},
        {"lt",  [](const variable &a, const variable &b) { return a < b; }},
        {"gt",  [](const variable &a, const variable &b) { return a > b; }},
        {"leq", [](const variable &a, const variable &b) { return a <= b; }},
        {"geq", [](const variable &a, const variable &b) { return a >= b; }},
        {"mod", [](int a, int b) { return a % b; }},
        {"add", [](const variable &a, const variable &b) { return a + b; }},
        {"sub", [](const variable &a, const variable &b) { return a - b; }},
        {"mul", [](const variable &a, const variable &b) { return a * b; }},
        {"div", [](const variable &a, const variable &b) { return a / b; }},
        {"abs", [](const variable &a) { return a > 0 ? a : -a; }},
        {"neg", [](const variable &a) { return -a; }},
        {"not", [](bool a) { return !a; }},
        {"and", [](bool a, bool b) { return a && b; }},
        {"or",  [](bool a, bool b) { return a || b; }},
        {"null", []{ return variable(); }},
        {"isnull", [](const variable &var) { return var.is_null(); }},
        {"hex", [](int num) {
            return fmt::format("{:x}", num);
        }},
        {"aggregate", [](const std::locale &loc, const std::vector<std::string> list) {
            variable ret;
            for (const auto &s : list) {
                ret += parse_num(loc, s);
            }
            return ret;
        }},
        {"lines", [](std::string &&str) {
            auto view = util::string_split(str, '\n');
            return std::vector<std::string>(view.begin(), view.end());
        }},
        {"list", [](varargs<variable> args) {
            return std::vector<variable>(args.begin(), args.end());
        }},
        {"subitem", [](const std::vector<variable> &vec, size_t index) {
            if (index >= vec.size()) return variable();
            return vec[index];
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
        {"table_header", [](std::string_view header, varargs<std::string_view> labels) {
            return table_header(header, labels);
        }},
        {"table_row", [](std::string_view row, std::vector<std::string> indices) {
            return table_row(row, indices);
        }},
        {"search", [](const std::locale &loc, std::string_view str, const std::string &regex, optional_size<1> index) -> variable {
            return std::string(search_regex(loc, str, regex, index));
        }},
        {"searchpos", [](const std::locale &loc, std::string_view str, const std::string &regex, optional_size<0> index) {
            return search_regex(loc, str, regex, index).begin() - str.begin();
        }},
        {"searchposend", [](const std::locale &loc, std::string_view str, const std::string &regex, optional_size<0> index) {
            return search_regex(loc, str, regex, index).end() - str.begin();
        }},
        {"search_all", [](const std::locale &loc, std::string_view str, const std::string &regex, optional_size<1> index) {
            return search_regex_all(loc, str, regex, index);
        }},
        {"captures", [](const std::locale &loc, std::string_view str, const std::string &regex) {
            return search_regex_captures(loc, str, regex);
        }},
        {"matches", [](const std::locale &loc, std::string_view str, const std::string &regex) {
            return std::regex_match(str.begin(), str.end(), create_regex(loc, regex));
        }},
        {"replace", [](std::string &&str, std::string_view from, std::string_view to) {
            return util::string_replace(str, from, to);
        }},
        {"date_regex", [](std::string_view format) {
            return date_regex(format);
        }},
        {"date", [](const std::locale &loc, std::string_view str, optional<std::string> format) {
            if (format.empty()) {
                return datetime::from_string(str);
            } else {
                return datetime::parse_date(loc, str, format);
            }
        }},
        {"search_date", [](const std::locale &loc, std::string_view str, const std::string &format, optional<std::string> regex, optional_size<1> index) {
            return search_date(loc, str, format, regex, index);
        }},
        {"date_format", [](const std::locale &loc, datetime date, const std::string &format) {
            return date.format(loc, format);
        }},
        {"ymd", [](int year, int month, int day) {
            return datetime::from_ymd(year, month, day);
        }},
        {"year_add", [](datetime date, int years) {
            date.add_years(years);
            return date;
        }},
        {"month_add", [](datetime date, int months) {
            date.add_months(months);
            return date;
        }},
        {"day_add", [](datetime date, int num) {
            date.add_days(num);
            return date;
        }},
        {"day_set", [](datetime date, int day) {
            date.set_day(day);
            return date;
        }},
        {"first_day", [](datetime date) {
            date.set_day(1);
            return date;
        }},
        {"last_day", [](datetime date) {
            date.set_to_last_month_day();
            return date;
        }},
        {"date_between", [](datetime date, datetime date_begin, datetime date_end) {
            return date >= date_begin && date <= date_end;
        }},
        {"singleline", [](std::string_view str) {
            return string_singleline(str);
        }},
        {"if", [](bool condition, const variable &var_if, optional<variable> var_else) {
            return condition ? var_if : var_else;
        }},
        {"ifnot", [](bool condition, const variable &var_if, optional<variable> var_else) {
            return condition ? var_else : var_if;
        }},
        {"trim", [](std::string_view str) {
            return util::string_trim(str);
        }},
        {"lpad", [](std::string_view str, int amount) {
            return fmt::format("{0: >{1}}", str, str.size() + amount);
        }},
        {"rpad", [](std::string_view str, int amount) {
            return fmt::format("{0: <{1}}", str, str.size() + amount);
        }},
        {"contains", [](std::string_view str, std::string_view str2) {
            return string_find_icase(str, str2, 0).begin() != str.end();
        }},
        {"substr", [](std::string_view str, size_t pos, optional_size<std::string_view::npos> count) {
            return std::string(str.substr(std::min(str.size(), pos), count));
        }},
        {"between", [](const std::locale &loc, std::string_view str, const variable &from, optional<variable> to) {
            return string_between(loc, str, from, to, false, false);
        }},
        {"lbetween", [](const std::locale &loc, std::string_view str, const variable &from, optional<variable> to) {
            return string_between(loc, str, from, to, true, false);
        }},
        {"rbetween", [](const std::locale &loc, std::string_view str, const variable &from, optional<variable> to) {
            return string_between(loc, str, from, to, false, true);
        }},
        {"lrbetween", [](const std::locale &loc, std::string_view str, const variable &from, optional<variable> to) {
            return string_between(loc, str, from, to, true, true);
        }},
        {"strcat", [](varargs<std::string_view> args) {
            return util::string_join(args);
        }},
        {"strlen", [](std::string_view str) {
            return str.size();
        }},
        {"indexof", [](std::string_view str, std::string_view value, optional<size_t> index) {
            return string_find_icase(str, value, index).begin() - str.begin();
        }},
        {"indexofend", [](std::string_view str, std::string_view value, optional<size_t> index) {
            return string_find_icase(str, value, index).end() - str.begin();
        }},
        {"tolower", [](const std::locale &loc, std::string_view str) {
            return boost::locale::to_lower(str.data(), str.data() + str.size(), loc);
        }},
        {"toupper", [](const std::locale &loc, std::string_view str) {
            return boost::locale::to_upper(str.data(), str.data() + str.size(), loc);
        }},
        {"totitle", [](const std::locale &loc, std::string_view str) {
            return boost::locale::to_title(str.data(), str.data() + str.size(), loc);
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
        {"readfile", [](const std::string &filename) -> variable {
            std::ifstream ifs(filename);
            if (ifs.fail()) return {};
            return std::string{
                std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>()};
        }},
    };
}