#include "functions.h"
#include "utils.h"

#include <optional>

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &convert_var<variable &>    (variable &var) { return var; }
template<> inline std::string &convert_var<std::string &> (variable &var) { return var.str(); }

template<> inline std::string_view  convert_var<std::string_view> (variable &var) { return var.str_view(); }
template<> inline fixed_point  convert_var<fixed_point> (variable &var) { return var.number(); }
template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.number().getAsDouble(); }
template<> inline double       convert_var<double>     (variable &var) { return var.number().getAsDouble(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_convertible = requires(variable &v) {
    convert_var<T>(v);
};

template<typename T> struct is_variable : std::bool_constant<
    is_convertible<std::decay_t<T>> ||
    is_convertible<std::add_lvalue_reference_t<std::decay_t<T>>>> {};

template<typename T> struct is_optional_impl : std::false_type {};
template<typename T> struct is_optional_impl<std::optional<T>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_optional : is_optional_impl<std::decay_t<T>> {};

template<typename T> struct is_vector_impl : std::false_type {};
template<typename T> struct is_vector_impl<std::vector<T>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_vector : is_vector_impl<std::decay_t<T>> {};

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
    static constexpr bool vec = is_vector<First>{};

    // Req è false se è stato trovato un std::optional<T>
    using recursive = check_args_impl<Req ? var : !opt, Ts ...>;

public:
    // Conta il numero di tipi che non sono std::optional<T>
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : 0;
    // Conta il numero totale di tipi o -1 (INT_MAX) se l'ultimo è std::vector<T>
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

template<typename T, typename InputIt, typename Function>
constexpr std::vector<T> transformed_vector(InputIt begin, InputIt end, Function fun) {
    std::vector<T> ret;
    for (;begin!=end; ++begin) {
        ret.emplace_back(std::move(fun(*begin)));
    }
    return ret;
}

template<typename T> using convert_type = std::conditional_t<
    is_convertible<std::add_lvalue_reference_t<std::decay_t<T>>>,
        std::add_lvalue_reference_t<std::decay_t<T>>,
        std::decay_t<T>>;

template<typename TypeList, size_t I> inline get_nth_t<I, TypeList> get_arg(arg_list &args) {
    using type = convert_type<get_nth_t<I, TypeList>>;
    if constexpr (is_optional<type>{}) {
        using opt_type = typename type::value_type;
        if (I >= args.size()) {
            return std::nullopt;
        } else {
            return std::move(convert_var<convert_type<opt_type>>(args[I]));
        }
    } else if constexpr (is_vector<type>{}) {
        using vec_type = typename type::value_type;
        return transformed_vector<vec_type>(args.begin() + I, args.end(), convert_var<convert_type<vec_type>>);
    } else {
        return std::move(convert_var<type>(args[I]));
    }
}

template<typename Function>
inline std::pair<std::string, function_handler> create_function(const std::string &name, Function fun) {
    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template

    using fun_args = check_args<decltype(+fun)>;
    static_assert(fun_args::valid);

    // Viene creata una closure che passa automaticamente gli argomenti
    // da arg_list alla funzione fun, convertendoli nei tipi giusti

    function_handler ret([fun](arg_list &&args) {
        return [&] <std::size_t ... Is> (std::index_sequence<Is...>) {
            return fun(get_arg<typename fun_args::types, Is>(args) ...);
        }(std::make_index_sequence<fun_args::types::size>{});
    });

    ret.name = name;
    ret.minargs = fun_args::minargs;
    ret.maxargs = fun_args::maxargs;

    return {name, std::move(ret)};
}

static const std::unordered_map<std::string, function_handler> lookup {
    create_function("search", [](std::string_view str, std::string_view regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    create_function("matches", [](std::string_view str, std::string_view regex, std::optional<int> index) {
        return search_regex_all(regex, str, index.value_or(1));
    }),
    create_function("captures", [](std::string_view str, std::string_view regex) {
        return search_regex_captures(regex, str);
    }),
    create_function("date", [](std::string_view str, const std::string &format, std::optional<std::string_view> regex, std::optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("month", [](std::string_view str, std::optional<std::string> format, std::optional<std::string_view> regex, std::optional<int> index) {
        return parse_month(format.value_or(""), str, regex.value_or(""), index.value_or(1));
    }),
    create_function("replace", [](std::string &&value, std::string_view regex, const std::string &to) {
        return string_replace_regex(value, regex, to);
    }),
    create_function("date_format", [](std::string_view date, const std::string &format) {
        return date_format(date, format);
    }),
    create_function("month_add", [](std::string_view month, int num) {
        return date_month_add(month, num);
    }),
    create_function("singleline", [](std::string &&str) {
        return singleline(std::move(str));
    }),
    create_function("if", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }),
    create_function("ifnot", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_else.value_or(variable::null_var()) : var_if;
    }),
    create_function("contains", [](std::string_view str, std::string_view str2) {
        return string_findicase(str, str2, 0) < str.size();
    }),
    create_function("substr", [](std::string_view str, int pos, std::optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(std::string(str.substr(pos, count.value_or(std::string_view::npos))));
        }
        return variable::null_var();
    }),
    create_function("strlen", [](std::string_view str) {
        return str.size();
    }),
    create_function("indexof", [](std::string_view str, std::string_view value, std::optional<int> index) {
        return string_findicase(str, value, index.value_or(0));
    }),
    create_function("tolower", [](std::string &&str) {
        return string_tolower(std::move(str));
    }),
    create_function("toupper", [](std::string &&str) {
        return string_toupper(std::move(str));
    }),
    create_function("isempty", [](const variable &var) {
        return var.empty();
    }),
    create_function("percent", [](const std::string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable::null_var();
        }
    }),
    create_function("format", [](std::string_view format, std::vector<std::string> args) {
        return string_format(format, args);
    }),
    create_function("coalesce", [](std::vector<variable> args) {
        for (auto &arg : args) {
            if (!arg.empty()) return arg;
        }
        return variable::null_var();
    }),
    create_function("strcat", [](std::vector<std::string> args) {
        std::string var;
        for (auto &arg : args) {
            var += arg;
        }
        return var;
    }),
    create_function("max", [](std::vector<variable> args) {
        if (args.empty()) {
            return variable::null_var();
        }
        return std::move(*std::max_element(args.begin(), args.end()));
    }),
    create_function("min", [](std::vector<variable> args) {
        if (args.empty()) {
            return variable::null_var();
        }
        return std::move(*std::min_element(args.begin(), args.end()));
    })
};

const function_handler *find_function(const std::string &name) {
    auto it = lookup.find(name);
    if (it == lookup.end()) {
        return nullptr;
    } else {
        return &(it->second);
    }
}