#include "functions.h"

#include <regex>
#include <numeric>
#include <fstream>

#include <boost/locale.hpp>

#include "exceptions.h"
#include "utils.h"
#include "svstream.h"
#include "reader.h"
#include "exceptions.h"

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
                        throw layout_error(intl::format("INVALID_FORMAT_STRING", str));
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
            throw layout_error(intl::format("INVALID_REGEXP", regex, error.what()));
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
        return match | std::views::drop(1) | util::range_to<std::vector<std::string>>;
    }

    // cerca la regex in str e ritorna i valori trovati
    static std::vector<std::string> search_regex_all(const std::locale &loc, std::string_view value, const std::string &regex, size_t index) {
        auto reg = create_regex(loc, regex);
        return std::ranges::subrange(
            std::cregex_token_iterator(value.data(), value.data() + value.size(), reg, index),
            std::cregex_token_iterator())
            | util::range_to<std::vector<std::string>>;
    }

    template<std::ranges::input_range R>
    static std::vector<variable> table_header(std::string_view value, R &&labels) {
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
        return labels | std::views::transform([&, pos = header_str.data()](std::string_view label) mutable -> variable {
            std::cmatch match;
            if (std::regex_search(&*pos, header_str.data() + header_str.size(), match, std::regex(label.begin(), label.end(), std::regex::icase))) {
                auto match_str = match_to_view(match[0]);
                pos = std::find_if_not(match_str.data() + match_str.size(), header_str.data() + header_str.size(), isspace);
                if (pos == header_str.data() + header_str.size()) {
                    return std::vector{match_str.data() - header_str.data(), -1ll};
                } else {
                    return std::vector{match_str.data() - header_str.data(), pos - match_str.data()};
                }
            } else {
                return {};
            }
        }) | util::range_to_vector;
    }

    static std::vector<std::string> table_row(std::string_view row, const std::vector<std::vector<int>> &indices) {
        return indices | std::views::transform([&](const std::vector<int> &idx) {
            if (idx.size() == 2) {
                size_t begin = idx[0];
                if (begin < row.size()) {
                    size_t len = idx[1];
                    return row.substr(begin, len);
                }
            }
            return std::string_view();
        }) | util::range_to<std::vector<std::string>>;
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
                    throw std::invalid_argument(intl::format("INVALID_DATE_FORMAT"));
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

    template<bool OuterLeft, bool OuterRight>
    static std::string_view string_between(const std::locale &loc, std::string_view str, const variable &from, const variable &to) {
        auto search_range = [&](const variable &expr) -> std::string_view {
            if (expr.is_regex()) {
                return search_regex(loc, str, expr.as_string(), 0);
            } else {
                return string_find_icase(str, expr.as_view(), 0) | util::range_to<std::string_view>;
            }
        };
        if (!from.is_null()) {
            auto from_span = search_range(from);
            if constexpr (OuterLeft) {
                str = std::string_view(from_span.data(), str.data() + str.size());
            } else {
                str = std::string_view(from_span.data() + from_span.size(), str.data() + str.size());
            }
        }
        if (!to.is_null()) {
            auto to_span = search_range(to);
            if constexpr (OuterRight) {
                str = std::string_view(str.data(), to_span.data() + to_span.size());
            } else {
                str = std::string_view(str.data(), to_span.data());
            }
        }
        return str;
    }

    const function_map function_lookup::functions {
        {"str", [](const std::string &str) { return str; }},
        {"num", [](const reader *ctx, const variable &var) {
            if (var.is_number()) return var;
            return parse_num(ctx->m_locale, var.as_view());
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
        {"aggregate", [](const reader *ctx, const std::vector<std::string> &list) {
            return std::transform_reduce(list.begin(), list.end(), variable(), std::plus<>(), [&](const std::string &s) {
                return parse_num(ctx->m_locale, s);
            });
        }},
        {"lines", [](std::string_view str) {
            return util::string_split(str, '\n')
                | std::views::transform(util::string_trim)
                | std::views::filter(std::not_fn(&std::string::empty))
                | util::range_to_vector;
        }},
        {"list", [](varargs<variable> args) {
            return args | util::range_to_vector;
        }},
        {"subitem", [](const std::vector<variable> &vec, size_t index) {
            if (index >= vec.size()) return variable();
            return vec[index];
        }},
        {"sum", [](varargs<fixed_point> args) {
            return std::reduce(args.begin(), args.end());
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
        {"table_row", [](std::string_view row, std::vector<std::vector<int>> indices) {
            return table_row(row, indices);
        }},
        {"search", [](const reader *ctx, std::string_view str, const std::string &regex, optional_size<1> index) -> variable {
            return std::string(search_regex(ctx->m_locale, str, regex, index));
        }},
        {"searchpos", [](const reader *ctx, std::string_view str, const std::string &regex, optional_size<0> index) {
            return search_regex(ctx->m_locale, str, regex, index).begin() - str.begin();
        }},
        {"searchposend", [](const reader *ctx, std::string_view str, const std::string &regex, optional_size<0> index) {
            return search_regex(ctx->m_locale, str, regex, index).end() - str.begin();
        }},
        {"search_all", [](const reader *ctx, std::string_view str, const std::string &regex, optional_size<1> index) {
            return search_regex_all(ctx->m_locale, str, regex, index);
        }},
        {"captures", [](const reader *ctx, std::string_view str, const std::string &regex) {
            return search_regex_captures(ctx->m_locale, str, regex);
        }},
        {"matches", [](const reader *ctx, std::string_view str, const std::string &regex) {
            return std::regex_match(str.begin(), str.end(), create_regex(ctx->m_locale, regex));
        }},
        {"replace", [](std::string &&str, std::string_view from, std::string_view to) {
            return util::string_replace(str, from, to);
        }},
        {"date_regex", [](std::string_view format) {
            return date_regex(format);
        }},
        {"date", [](const reader *ctx, std::string_view str, optional<std::string> format) {
            if (format.empty()) {
                return datetime::from_string(str);
            } else {
                return datetime::parse_date(ctx->m_locale, str, format);
            }
        }},
        {"search_date", [](const reader *ctx, std::string_view str, const std::string &format, optional<std::string> regex, optional_size<1> index) {
            return search_date(ctx->m_locale, str, format, regex, index);
        }},
        {"date_format", [](const reader *ctx, datetime date, const std::string &format) {
            return date.format(ctx->m_locale, format);
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
        {"conditional", [](bool condition, const variable &var_if, optional<variable> var_else) {
            return condition ? var_if : var_else;
        }},
        {"conditional_not", [](bool condition, const variable &var_if, optional<variable> var_else) {
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
        {"between", [](const reader *ctx, std::string_view str, const variable &from, optional<variable> to) {
            return string_between<false, false>(ctx->m_locale, str, from, to);
        }},
        {"lbetween", [](const reader *ctx, std::string_view str, const variable &from, optional<variable> to) {
            return string_between<true, false>(ctx->m_locale, str, from, to);
        }},
        {"rbetween", [](const reader *ctx, std::string_view str, const variable &from, optional<variable> to) {
            return string_between<false, true>(ctx->m_locale, str, from, to);
        }},
        {"lrbetween", [](const reader *ctx, std::string_view str, const variable &from, optional<variable> to) {
            return string_between<true, true>(ctx->m_locale, str, from, to);
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
        {"tolower", [](const reader *ctx, std::string_view str) {
            return boost::locale::to_lower(str.data(), str.data() + str.size(), ctx->m_locale);
        }},
        {"toupper", [](const reader *ctx, std::string_view str) {
            return boost::locale::to_upper(str.data(), str.data() + str.size(), ctx->m_locale);
        }},
        {"totitle", [](const reader *ctx, std::string_view str) {
            return boost::locale::to_title(str.data(), str.data() + str.size(), ctx->m_locale);
        }},
        {"isempty", [](std::string_view str) {
            return str.empty();
        }},
        {"format", [](std::string_view format, varargs<std::string_view> args) {
            return string_format(format, args);
        }},
        {"coalesce", [](varargs<variable> args) {
            if (auto it = std::ranges::find_if_not(args, &variable::is_null); it != args.end()) return *it;
            return variable();
        }},
        {"readfile", [](const reader *ctx, const std::string &filename) -> variable {
            std::ifstream ifs(ctx->m_current_layout->parent_path() / filename);
            if (ifs.fail()) return {};
            return std::string{
                std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>()};
        }},
        {"box_page",        [](const reader *ctx) { return ctx->m_current_box.page; }},
        {"box_width",       [](const reader *ctx) { return ctx->m_current_box.w; }},
        {"box_height",      [](const reader *ctx) { return ctx->m_current_box.h; }},
        {"box_left",        [](const reader *ctx) { return ctx->m_current_box.x; }},
        {"box_top",         [](const reader *ctx) { return ctx->m_current_box.y; }},
        {"box_right",       [](const reader *ctx) { return ctx->m_current_box.x + ctx->m_current_box.w; }},
        {"box_bottom",      [](const reader *ctx) { return ctx->m_current_box.y + ctx->m_current_box.h; }},
        {"layout_filename", [](const reader *ctx) { return ctx->m_current_layout->string(); }},
        {"layout_dir",      [](const reader *ctx) { return ctx->m_current_layout->parent_path().string(); }},
        {"curtable",        [](const reader *ctx) { return std::ranges::distance(ctx->m_values.begin(), ctx->m_current_table); }},
        {"islasttable",     [](const reader *ctx) { return std::next(ctx->m_current_table) == ctx->m_values.end(); }},
        {"numtables",       [](const reader *ctx) { return ctx->m_values.size(); }},
        {"doc_numpages",    [](const reader *ctx) { return ctx->get_document().num_pages(); }},
        {"doc_filename",    [](const reader *ctx) { return ctx->get_document().filename().string(); }},
        {"ate",             [](const reader *ctx) { return ctx->m_current_box.page > ctx->get_document().num_pages(); }},
        {"curindex", [](const reader *ctx) -> variable{
            if (ctx->m_contents.empty()) {
                return {};
            } else {
                return ctx->m_contents.top().index();
            }
        }},
        
        {"error", [](const std::string &message, optional_int<-1> errcode) {
            throw layout_runtime_error(message, errcode);
        }},
        {"note", [](reader *ctx, const std::string &message) {
            ctx->m_notes.push_back(message);
        }},
        {"nexttable", [](reader *ctx) {
            if (std::next(ctx->m_current_table) == ctx->m_values.end()) {
                ctx->m_values.emplace_back();
            }
            ++ctx->m_current_table;
        }},
        {"cleartable", [](reader *ctx) {
            ctx->m_current_table->clear();
        }},
        {"firsttable", [](reader *ctx) {
            ctx->m_current_table = ctx->m_values.begin();
        }}
    };
}