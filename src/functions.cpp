#include "functions.h"

#include <regex>
#include <numeric>
#include <fstream>

#include <boost/locale.hpp>

#include "reader.h"

namespace bls {

    inline auto match_to_view(const std::csub_match &m) {
        return std::string_view(m.first, m.second);
    };

    // Converte una stringa in numero usando il formato del locale
    struct num_parser {
        const std::locale &loc;
        
        variable operator()(std::string_view str) const {
            if (str.empty()) return {};
            fixed_point num;
            util::isviewstream iss{str};
            iss.imbue(loc);
            if (dec::fromStream(iss, num)) {
                return num;
            } else {
                return {};
            }
        }

        variable operator()(const variable &var) const {
            if (var.is_null() || var.is_number()) return var;
            return (*this)(var.as_view());
        }

        variable operator()(const std::csub_match &m) const {
            return (*this)(match_to_view(m));
        }
    };

    // Formatta la stringa data, sostituendo $0 in fmt_args[0], $1 in fmt_args[1] e così via
    template<std::ranges::input_range R>
    static std::string string_format(std::string_view str, R &&fmt_args) {
        constexpr char FORMAT_CHAR = '$';
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
                        throw layout_error(intl::translate("INVALID_FORMAT_STRING", str));
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
            ret = std::format("\\d{{1,{0}}}(?:{1}\\d{{{0}}})*", int(*it), tho);
            for(++it; it != grp.rend(); ++it) {
                ret = std::format("(?:{2}{1}\\d{{{0}}}|\\d{{1,{0}}})", int(*it), tho, ret);
            }
            ret = std::format("(?:{}|\\d+)", ret);
        }
        return std::format("(?:-?{0}(?:{1}\\d+)?)(?!\\d)", ret, escape_regex_char(facet.decimal_point()));
    };

    // Costruisce un oggetto std::regex
    static std::regex create_regex(std::string_view regex) {
        try {
            return std::regex(regex.data(), regex.data() + regex.size(), std::regex::icase);
        } catch (const std::regex_error &error) {
            throw layout_error(std::format("{}\n{}\n{}", intl::translate("INVALID_REGEXP"), regex, error.what()));
        }
    }

    // Crea un oggetto regex dove \N corrisponde ad un numero
    static std::regex create_number_regex(const std::locale &loc, std::string_view regex) {
        return create_regex(util::string_replace(regex, "\\N", number_regex(loc)));
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

    // cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
    static std::string_view search_regex(std::string_view value, const std::regex &regex, size_t index) {
        std::cmatch match;
        if (std::regex_search(value.data(), value.data() + value.size(), match, regex)) {
            if (index < match.size()) return match_to_view(match[index]);
        }
        return {value.end(), value.end()};
    }

    // cerca la regex in str e ritorna tutti i capture del primo valore trovato
    static std::cmatch search_regex_captures(std::string_view value, const std::regex &regex) {
        std::cmatch match;
        if (std::regex_search(value.data(), value.data() + value.size(), match, regex)) {
            return match;
        }
        return {};
    }

    // cerca la regex in str e ritorna i valori trovati
    static auto search_regex_matches(std::string_view value, const std::regex &regex, size_t index) {
        return std::ranges::subrange(
            std::cregex_token_iterator(value.data(), value.data() + value.size(), regex, index),
            std::cregex_token_iterator());
    }

    template<std::ranges::input_range R>
    static variable table_header(std::string_view value, R &&labels) {
        std::cmatch header_match;
        std::regex header_regex(std::format(".*{}.*", util::string_join(std::ranges::transform_view(labels,
            [first=true](std::string_view str) mutable {
                if (first) {
                    first = false;
                    return std::string(str);
                } else {
                    return std::format("(?:{})?", str);
                }
            }), ".*")), std::regex::icase);
        if (!std::regex_search(value.data(), value.data() + value.size(), header_match, header_regex)) {
            return {};
        }
        auto header_str = match_to_view(header_match[0]);
        return labels | std::views::transform([&, pos = header_str.data()](std::string_view label) mutable -> variable {
            std::cmatch match;
            if (std::regex_search(pos, header_str.data() + header_str.size(), match, create_regex(label))) {
                auto match_str = match_to_view(match[0]);
                pos = std::find_if_not(match_str.data() + match_str.size(), header_str.data() + header_str.size(), isspace);
                if (pos == header_str.data() + header_str.size()) {
                    return variable_array{match_str.data() - header_str.data(), -1};
                } else {
                    return variable_array{match_str.data() - header_str.data(), pos - match_str.data()};
                }
            } else {
                return {};
            }
        });
    }

    static variable table_row(std::string_view row, vector_view<vector_view<int>> indices) {
        return indices | std::views::transform([&](vector_view<int> idx) -> variable {
            if (idx.size() == 2) {
                size_t begin = idx[0];
                if (begin < row.size()) {
                    return std::string(row.substr(begin, idx[1]));
                }
            }
            return {};
        });
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
                    throw layout_error(intl::translate("INVALID_DATE_FORMAT"));
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
    static variable search_date(const std::locale &loc, std::string_view value, const std::string &format, std::string_view regex, size_t index) {
        std::string regex_str;
        if (regex.empty()) {
            regex_str = date_regex(format);
            index = 0;
        } else {
            regex_str = util::string_replace(regex, "\\D", date_regex(format));
        }

        if (auto search_res = search_regex(value, create_regex(regex_str), index); !search_res.empty()) {
            return datetime::parse_date(loc, search_res, format);
        }
        return {};
    }

    template<bool OuterLeft, bool OuterRight>
    static std::string_view string_between(std::string_view str, string_state from, string_state to) {
        auto search_range = [&](string_state expr) -> std::string_view {
            if (expr.flags.is_regex) {
                return search_regex(str, create_regex(expr), 0);
            } else {
                return string_find_icase(str, expr, 0) | util::range_to<std::string_view>;
            }
        };
        if (!from.empty()) {
            auto from_span = search_range(from);
            if constexpr (OuterLeft) {
                str = std::string_view(from_span.data(), str.data() + str.size());
            } else {
                str = std::string_view(from_span.data() + from_span.size(), str.data() + str.size());
            }
        }
        if (!to.empty()) {
            auto to_span = search_range(to);
            if constexpr (OuterRight) {
                str = std::string_view(str.data(), to_span.data() + to_span.size());
            } else {
                str = std::string_view(str.data(), to_span.data());
            }
        }
        return str;
    }

    template<typename Function>
    variable_array zip(vector_view<variable> lhs, vector_view<variable> rhs, Function &&fun) {
        variable_array ret;
        size_t len = std::max(lhs.size(), rhs.size());
        ret.reserve(len);
        for (size_t i=0; i<len; ++i) {
            if (i < lhs.size()) {
                if (i < rhs.size()) {
                    ret.push_back(fun(lhs[i], rhs[i]));
                } else {
                    ret.push_back(fun(lhs[i], variable()));
                }
            } else {
                ret.push_back(fun(variable(), rhs[i]));
            }
        }
        return ret;
    }

    constexpr auto sv_to_string = [](std::string_view str) { return std::string(str); };

    const function_map function_lookup::functions {
        {"copy", [](const variable &var) { return var; }},
        {"required", [](const variable &var) {
            if (var.is_null()) {
                throw layout_error(intl::translate("FIELD_IS_REQUIRED"));
            } else {
                return var;
            }
        }},
        {"type", [](const variable &var) { return enums::to_string(var.type()); }},
        {"str", [](const std::string &str) { return str; }},
        {"num", [](const reader *ctx, const variable &var) {
            return num_parser{ctx->m_locale}(var);
        }},
        {"nums", [](const reader *ctx, vector_view<variable> vars) {
            return vars | std::views::transform(num_parser{ctx->m_locale});
        }},
        {"neg", [](const reader *ctx, const variable &var) {
            return -num_parser{ctx->m_locale}(var);
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
        {"not", [](bool a) { return !a; }},
        {"and", [](bool a, bool b) { return a && b; }},
        {"or",  [](bool a, bool b) { return a || b; }},
        {"isnull", [](const variable &var) { return var.is_null(); }},
        {"isempty", [](const variable &var) { return var.is_empty(); }},
        {"hex", [](int num) {
            return std::format("{:x}", num);
        }},
        {"split", [](std::string_view str, std::string_view separator) {
            return util::string_split(str, separator) | std::views::transform(sv_to_string);
        }},
        {"join", [](vector_view<std::string_view> strings, std::string_view separator) {
            return util::string_join(strings, separator);
        }},
        {"list", [](varargs<variable> args) {
            return args;
        }},
        {"enumerate", [](vector_view<variable> args) {
            return args | std::views::transform([i=0](const variable &var) mutable {
                return variable_array{i++, var};
            });
        }},
        {"zip_add", [](vector_view<variable> lhs, vector_view<variable> rhs) {
            return zip(lhs, rhs, std::plus<>{});
        }},
        {"zip_sub", [](vector_view<variable> lhs, vector_view<variable> rhs) {
            return zip(lhs, rhs, std::minus<>{});
        }},
        {"zip_mul", [](vector_view<variable> lhs, vector_view<variable> rhs) {
            return zip(lhs, rhs, std::multiplies<>{});
        }},
        {"sum", [](vector_view<variable> args) {
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
        {"clamp", [](const variable &value, const variable &low, const variable &high) {
            return std::clamp(value, low, high);
        }},
        {"repeated", [](const variable &value, size_t num) -> variable {
            if (value.is_null()) return {};
            return variable_array(num, value);
        }},
        {"range", [](vector_view<variable> vec, size_t index, size_t size) {
            return std::ranges::take_view(std::ranges::drop_view(vec, index), size);
        }},
        {"percent", [](std::string_view str) -> variable {
            if (!str.empty()) {
                return std::string(str) + "%";
            } else {
                return {};
            }
        }},
        {"table_header", [](std::string_view header, varargs<std::string_view> labels) {
            return table_header(header, labels);
        }},
        {"table_row", [](std::string_view row, vector_view<vector_view<int>> indices) {
            return table_row(row, indices);
        }},
        {"number_regex", [](const reader *ctx) {
            return number_regex(ctx->m_locale);
        }},
        {"search", [](std::string_view str, std::string_view regex, optional_size<1> index) -> variable {
            if (auto res = search_regex(str, create_regex(regex), index); !res.empty()) {
                return std::string(res);
            } else {
                return {};
            }
        }},
        {"search_num", [](const reader *ctx, std::string_view str, std::string_view regex, optional_size<1> index) {
            return num_parser{ctx->m_locale}(search_regex(str, create_number_regex(ctx->m_locale, regex), index));
        }},
        {"searchpos", [](std::string_view str, std::string_view regex, optional_size<0> index) {
            return search_regex(str, create_regex(regex), index).begin() - str.begin();
        }},
        {"searchposend", [](std::string_view str, std::string_view regex, optional_size<0> index) {
            return search_regex(str, create_regex(regex), index).end() - str.begin();
        }},
        {"matches", [](std::string_view str, std::string_view regex_str, optional_size<1> index) -> variable {
            auto regex = create_regex(regex_str);
            return search_regex_matches(str, regex, index) | std::views::transform(&std::csub_match::str);
        }},
        {"matches_num", [](const reader *ctx, std::string_view str, std::string_view regex_str, optional_size<1> index) -> variable {
            auto regex = create_number_regex(ctx->m_locale, regex_str);
            return search_regex_matches(str, regex, index)
                | std::views::transform(num_parser{ctx->m_locale});
        }},
        {"captures", [](std::string_view str, std::string_view regex_str) -> variable {
            auto match = search_regex_captures(str, create_regex(regex_str));
            return match
                | std::views::drop(1)
                | std::views::transform(&std::csub_match::str);
        }},
        {"captures_num", [](const reader *ctx, std::string_view str, std::string_view regex_str) -> variable {
            auto match = search_regex_captures(str, create_number_regex(ctx->m_locale, regex_str));
            return match
                | std::views::drop(1)
                | std::views::transform(num_parser{ctx->m_locale});
        }},
        {"ismatch", [](std::string_view str, std::string_view regex) {
            return std::regex_match(str.begin(), str.end(), create_regex(regex));
        }},
        {"replace", [](std::string_view str, std::string_view from, std::string_view to) {
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
        {"search_date", [](const reader *ctx, std::string_view str, const std::string &format, optional<std::string_view> regex, optional_size<1> index) {
            return search_date(ctx->m_locale, str, format, regex, index);
        }},
        {"search_month", [](const reader *ctx, std::string_view str, const std::string &format, optional<std::string_view> regex, optional_size<1> index) -> variable {
            variable var = search_date(ctx->m_locale, str, format, regex, index);
            if (!var.is_null()) {
                datetime date = var.as_date();
                date.set_day(1);
                return date;
            }
            return {};
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
        {"isbetween", [](const variable &var, const variable &min, const variable &max) {
            return var >= min && var <= max;
        }},
        {"singleline", [](std::string_view str) {
            return string_singleline(str);
        }},
        {"trim", [](std::string_view str) {
            return util::string_trim(str);
        }},
        {"lpad", [](std::string_view str, int amount) {
            return std::format("{0: >{1}}", str, str.size() + amount);
        }},
        {"rpad", [](std::string_view str, int amount) {
            return std::format("{0: <{1}}", str, str.size() + amount);
        }},
        {"contains", [](std::string_view str, std::string_view str2) {
            return string_find_icase(str, str2, 0).begin() != str.end();
        }},
        {"substr", [](std::string_view str, size_t pos, optional_size<std::string_view::npos> count) {
            return std::string(str.substr(std::min(str.size(), pos), count));
        }},
        {"between", [](std::string_view str, string_state from, optional<string_state> to) {
            return string_between<false, false>(str, from, to);
        }},
        {"lbetween", [](std::string_view str, string_state from, optional<string_state> to) {
            return string_between<true, false>(str, from, to);
        }},
        {"rbetween", [](std::string_view str, string_state from, optional<string_state> to) {
            return string_between<false, true>(str, from, to);
        }},
        {"lrbetween", [](std::string_view str, string_state from, optional<string_state> to) {
            return string_between<true, true>(str, from, to);
        }},
        {"strcat", [](varargs<std::string_view> args) {
            return util::string_join(args);
        }},
        {"size", [](const variable &var) {
            return var.size();
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
        {"format", [](std::string_view format, varargs<std::string_view> args) {
            return string_format(format, args);
        }},
        {"coalesce", [](varargs<variable> args) -> variable {
            if (auto it = std::ranges::find_if_not(args, &variable::is_null); it != args.end()) return *it;
            return {};
        }},
        {"readfile", [](const reader *ctx, std::string_view filename) -> variable {
            std::ifstream ifs(ctx->m_current_layout->parent_path() / std::filesystem::path(filename.begin(), filename.end()));
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
        
        {"error", [](std::string_view message, optional_int<-1> errcode) {
            throw scripted_error(std::string(message), errcode);
        }},
        {"note", [](reader *ctx, std::string_view message) {
            ctx->m_notes.push_back(std::string(message));
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