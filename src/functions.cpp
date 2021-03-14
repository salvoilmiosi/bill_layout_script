#include "functions.h"
#include "utils.h"

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

        fixed_point num;
        if (parse_num(num, var.str_view())) {
            return variable(num);
        } else {
            return variable::null_var();
        }
    }},
    {"aggregate", [](std::string_view str) {
        auto view = string_split(str, RESULT_SEPARATOR);
        return std::accumulate(view.begin(), view.end(), variable(), [](const variable &a, std::string_view b) {
            fixed_point num;
            if (parse_num(num, b)) {
                return variable(a.number() + num);
            }
            return a;
        });
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
        string_replace(str, from, to);
        return str;
    }},
    {"date_format", [](time_t date, const std::string &format) {
        return date_format(date, format);
    }},
    {"month_add", [](time_t date, int num) {
        return date_add_month(date, num);
    }},
    {"last_day", [](time_t month) {
        return date_last_day(month);
    }},
    {"date_between", [](time_t date, time_t date_begin, time_t date_end) {
        return date_is_between(date, date_begin, date_end);
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
        return string_tolower(str);
    }},
    {"toupper", [](std::string_view str) {
        return string_toupper(str);
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