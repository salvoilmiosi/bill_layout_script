#include "functions.h"
#include "utils.h"
#include "intl.h"

#include <optional>
#include <algorithm>
#include <numeric>

template<typename T> struct is_variable : std::bool_constant<! std::is_void_v<convert_rvalue<T>>> {};

template<typename T> struct is_optional_impl : std::false_type {};
template<typename T> struct is_optional_impl<std::optional<T>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_optional : is_optional_impl<std::decay_t<T>> {};

template<typename T> struct is_varargs_impl : std::false_type {};
template<typename T> struct is_varargs_impl<varargs<T>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_varargs : is_varargs_impl<std::decay_t<T>> {};

template<typename ... Ts> struct type_list {
    static constexpr size_t size = sizeof...(Ts);
};

template<size_t N, typename TypeList> struct get_nth {};

template<size_t N, typename First, typename ... Ts> struct get_nth<N, type_list<First, Ts...>> {
    using type = typename get_nth<N-1, type_list<Ts ...>>::type;
};

template<typename First, typename ... Ts> struct get_nth<0, type_list<First, Ts ...>> {
    using type = First;
};

template<size_t I, typename TypeList> using get_nth_t = typename get_nth<I, TypeList>::type;

template<bool Req, typename ... Ts> struct check_args_impl {};
template<bool Req> struct check_args_impl<Req> {
    static constexpr bool minargs = 0;
    static constexpr bool maxargs = 0;

    static constexpr bool valid = true;
};

template<bool Req, typename First, typename ... Ts>
class check_args_impl<Req, First, Ts ...> {
private:
    static constexpr bool var = is_variable<First>{};
    static constexpr bool opt = is_optional<First>{};
    static constexpr bool vec = is_varargs<First>{};

    // Req è false se è stato trovato un std::optional<T>
    using recursive = check_args_impl<Req ? var : !opt, Ts ...>;

public:
    // Conta il numero di tipi che non sono std::optional<T>
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : 0;
    // Conta il numero totale di tipi o -1 (INT_MAX) se l'ultimo è varargs<T>
    static constexpr size_t maxargs = (vec || recursive::maxargs == std::numeric_limits<size_t>::max()) ?
        std::numeric_limits<size_t>::max() : 1 + recursive::maxargs;

    // true solo se i tipi seguono il pattern (var*N, opt*M, [vec])
    // Se Req==false e var==true, valid=false
    static constexpr bool valid = vec ? sizeof...(Ts) == 0 : (Req || opt) && recursive::valid;
};

template<typename Function> struct check_args {};
template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : check_args_impl<true, Ts ...> {
    using types = type_list<Ts ...>;
};

template<typename TypeList, size_t I> inline decltype(auto) get_arg(arg_list &args) {
    using type = std::decay_t<get_nth_t<I, TypeList>>;
    if constexpr (is_optional<type>{}) {
        using opt_type = typename type::value_type;
        if (I >= args.size()) {
            return std::optional<opt_type>(std::nullopt);
        } else {
            return std::optional<opt_type>(convert_var<convert_rvalue<opt_type>>(args[I]));
        }
    } else if constexpr (is_varargs<type>{}) {
        return varargs<typename type::var_type>(args.subspan(I));
    } else {
        return convert_var<convert_rvalue<type>>(args[I]);
    }
}

template<typename Function>
function_handler::function_handler(Function fun) {
    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template

    using fun_args = check_args<decltype(+fun)>;
    static_assert(fun_args::valid);

    // Viene creata una closure che passa automaticamente gli argomenti
    // da arg_list alla funzione fun, convertendoli nei tipi giusti

    function_base::operator = ([fun](arg_list &&args) -> variable {
        return [&] <std::size_t ... Is> (std::index_sequence<Is...>) {
            return fun(get_arg<typename fun_args::types, Is>(args) ...);
        }(std::make_index_sequence<fun_args::types::size>{});
    });

    minargs = fun_args::minargs;
    maxargs = fun_args::maxargs;
}


static bool parse_num(fixed_point &num, std::string_view str) {
    std::istringstream iss;
    iss.rdbuf()->pubsetbuf(const_cast<char *>(str.begin()), str.size());
    return dec::fromStream(iss, dec::decimal_format(intl::decimal_point(), intl::thousand_sep()), num);
};

const std::map<std::string_view, function_handler> function_lookup {
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
        variable ret;
        for (const auto &view : string_split(str, {&RESULT_SEPARATOR, 1})) {
            fixed_point num;
            if (parse_num(num, view)) {
                ret += num;
            }
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
        return search_regex_all(regex, str, index.value_or(1));
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
    {"month_add", [](time_t month, int num) {
        return date_month_add(month, num);
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
        return string_findicase(str, str2, 0) < str.size();
    }},
    {"substr", [](std::string_view str, int pos, std::optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(std::string(str.substr(pos, count.value_or(std::string_view::npos))));
        }
        return variable::null_var();
    }},
    {"strcat", [](varargs<std::string_view> args) {
        std::string var;
        for (const auto &arg : args) {
            var.append(arg.begin(), arg.end());
        }
        return var;
    }},
    {"strlen", [](std::string_view str) {
        return str.size();
    }},
    {"indexof", [](std::string_view str, std::string_view value, std::optional<int> index) {
        return string_findicase(str, value, index.value_or(0));
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