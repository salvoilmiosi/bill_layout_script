#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <span>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

using std::string;
using std::string_view;
using std::optional;
using std::vector;

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &convert_var<variable &>    (variable &var) { return var; }
template<> inline string      &convert_var<string &>      (variable &var) { return var.str(); }

template<> inline string_view  convert_var<string_view> (variable &var) { return var.str_view(); }
template<> inline fixed_point  convert_var<fixed_point> (variable &var) { return var.number(); }
template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.number().getAsDouble(); }
template<> inline double       convert_var<double>     (variable &var) { return var.number().getAsDouble(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_convertible = requires(variable &v) {
    convert_var<T>(v);
};

template<typename T> constexpr bool is_variable =
    is_convertible<std::decay_t<T>> ||
    is_convertible<std::add_lvalue_reference_t<std::decay_t<T>>>;

template<typename T> struct is_optional_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_optional_impl<optional<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_optional = is_optional_impl<std::decay_t<T>>::value;

template<typename T> struct is_vector_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_vector_impl<vector<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_vector = is_vector_impl<std::decay_t<T>>::value;

template<size_t N, typename First, typename ... Ts> struct get_nth {
    using type = typename get_nth<N-1, Ts ...>::type;
};

template<typename First, typename ... Ts> struct get_nth<0, First, Ts ...> {
    using type = First;
};

template<typename ... Ts> struct type_list {
    static constexpr size_t size = sizeof...(Ts);
    template<size_t I> using get = typename get_nth<I, Ts ...>::type;
};

template<> struct type_list<> {
    static constexpr size_t size = 0;
};

template<bool Req, typename ... Ts> struct check_args_impl {};
template<bool Req> struct check_args_impl<Req> {
    static constexpr bool minargs = 0;
    static constexpr bool maxargs = 0;

    static constexpr bool valid = true;
};

template<bool Req, typename First, typename ... Ts>
class check_args_impl<Req, First, Ts ...> {
private:
    static constexpr bool var = is_variable<First>;
    static constexpr bool opt = is_optional<First>;
    static constexpr bool vec = is_vector<First>;

    // Req è false se è stato trovato un optional<T>
    using recursive = check_args_impl<Req ? var : !opt, Ts ...>;

public:
    // Conta il numero di tipi che non sono optional<T>
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : 0;
    // Conta il numero totale di tipi o -1 (INT_MAX) se l'ultimo è vector<T>
    static constexpr size_t maxargs = (vec || recursive::maxargs == std::numeric_limits<size_t>::max()) ?
        std::numeric_limits<size_t>::max() : 1 + recursive::maxargs;

    // true solo se i tipi seguono il pattern (var*N, opt*M, [vec])
    // Se Req==false e var==true, valid=false
    static constexpr bool valid = vec ? sizeof...(Ts) == 0 : (Req || opt) && recursive::valid;
};

using arg_list = std::span<variable>;

template<typename T> using convert_type = std::conditional_t<
    is_convertible<std::add_lvalue_reference_t<std::decay_t<T>>>,
        std::add_lvalue_reference_t<std::decay_t<T>>,
        std::decay_t<T>>;

template<typename Function> struct check_args {};
template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : public check_args_impl<true, Ts ...> {
    using types = type_list<Ts ...>;
};

template<typename T, typename InputIt, typename Function>
constexpr vector<T> transformed_vector(InputIt begin, InputIt end, Function fun) {
    vector<T> ret;
    for (;begin!=end; ++begin) {
        ret.emplace_back(std::move(fun(*begin)));
    }
    return ret;
}

template<typename TypeList, size_t I> inline typename TypeList::get<I> get_arg(arg_list &args) {
    using type = convert_type<typename TypeList::get<I>>;
    if constexpr (is_optional<type>) {
        using opt_type = typename type::value_type;
        if (I >= args.size()) {
            return std::nullopt;
        } else {
            return std::move(convert_var<convert_type<opt_type>>(args[I]));
        }
    } else if constexpr (is_vector<type>) {
        using vec_type = typename type::value_type;
        return transformed_vector<vec_type>(args.begin() + I, args.end(), convert_var<convert_type<vec_type>>);
    } else {
        return std::move(convert_var<type>(args[I]));
    }
}

template<typename TypeList, typename Function, std::size_t ... Is>
constexpr variable exec_helper(Function fun, arg_list &args, std::index_sequence<Is ...>) {
    return fun(get_arg<TypeList, Is>(args)...);
}

void check_numargs(const string &name, size_t numargs, size_t minargs, size_t maxargs) {
    if (numargs < minargs || numargs > maxargs) {
        if (maxargs == std::numeric_limits<size_t>::max()) {
            throw layout_error(fmt::format("La funzione {0} richiede almeno {1} argomenti", name, minargs));
        } else if (minargs == maxargs) {
            throw layout_error(fmt::format("La funzione {0} richiede {1} argomenti", name, minargs));
        } else {
            throw layout_error(fmt::format("La funzione {0} richiede {1}-{2} argomenti", name, minargs, maxargs));
        }
    }
}

using function_handler = std::function<variable(arg_list&&)>;

template<typename Function>
constexpr std::pair<string, function_handler> create_function(const string &name, Function fun) {
    using fun_args = check_args<decltype(+fun)>;
    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template
    static_assert(fun_args::valid);
    // Viene creata una closure che automaticamente controlla
    // il numero di argomenti passati alla funzione e li passa
    // negli argomenti di fun, convertendoli nei tipi giusti
    return {name, [name, fun](arg_list &&args) {
        check_numargs(name, args.size(), fun_args::minargs, fun_args::maxargs);
        return exec_helper<typename fun_args::types>(fun, args,
            std::make_index_sequence<fun_args::types::size>{});
    }};
}

static const std::unordered_map<string, function_handler> lookup {
    create_function("search", [](string_view str, const string &regex, optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    create_function("searchall", [](string_view str, const string &regex, optional<int> index) {
        return string_join(search_regex_all(regex, str, index.value_or(1)), "\n");
    }),
    create_function("date", [](string_view str, const string &format, optional<string> regex, optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("month", [](string_view str, optional<string> format, optional<string> regex, optional<int> index) {
        return parse_month(format.value_or(""), str, regex.value_or(""), index.value_or(1));
    }),
    create_function("replace", [](string &&value, const string &regex, const string &to) {
        return string_replace_regex(value, regex, to);
    }),
    create_function("date_format", [](string_view date, const string &format) {
        return date_format(date, format);
    }),
    create_function("month_add", [](string_view month, int num) {
        return date_month_add(month, num);
    }),
    create_function("nonewline", [](string &&str) {
        return nonewline(std::move(str));
    }),
    create_function("if", [](bool condition, const variable &var_if, optional<variable> var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }),
    create_function("ifnot", [](bool condition, const variable &var_if, optional<variable> var_else) {
        return condition ? var_else.value_or(variable::null_var()) : var_if;
    }),
    create_function("contains", [](string_view str, string_view str2) {
        return string_findicase(str, str2, 0) < str.size();
    }),
    create_function("substr", [](string_view str, int pos, optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(std::string(str.substr(pos, count.value_or(string_view::npos))));
        }
        return variable::null_var();
    }),
    create_function("strlen", [](string_view str) {
        return str.size();
    }),
    create_function("indexof", [](string_view str, string_view value, optional<int> index) {
        return string_findicase(str, value, index.value_or(0));
    }),
    create_function("tolower", [](string &&str) {
        return string_tolower(std::move(str));
    }),
    create_function("toupper", [](string &&str) {
        return string_toupper(std::move(str));
    }),
    create_function("isempty", [](const variable &var) {
        return var.empty();
    }),
    create_function("percent", [](const string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable::null_var();
        }
    }),
    create_function("format", [](string format, vector<string> args) {
        for (size_t i=0; i<args.size(); ++i) {
            string_replace(format, fmt::format("${}", i), args[i]);
        }
        return format;
    }),
    create_function("coalesce", [](vector<variable> args) {
        for (auto &arg : args) {
            if (!arg.empty()) return arg;
        }
        return variable::null_var();
    }),
    create_function("strcat", [](vector<string> args) {
        string var;
        for (auto &arg : args) {
            var += arg;
        }
        return var;
    }),
    create_function("null", []{
        return variable::null_var();
    })
};

void reader::call_function(const string &name, size_t numargs) {
    auto it = lookup.find(name);
    if (it != lookup.end()) {
        variable ret = it->second(arg_list(
            m_con.vars.end() - numargs,
            m_con.vars.end()));
        m_con.vars.resize(m_con.vars.size() - numargs);
        m_con.vars.push(std::move(ret));
    } else {
        throw layout_error(fmt::format("Funzione sconosciuta: {0}", name));
    }
}