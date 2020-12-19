#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

using arg_list = std::vector<variable>;

template<typename T> T convert_var(variable &var) = delete;

template<> const variable    &convert_var<const variable &>    (variable &var) { return var; }
template<> const std::string &convert_var<const std::string &> (variable &var) { return var.str(); }
template<> const fixed_point &convert_var<const fixed_point &> (variable &var) { return var.number(); }

template<> variable     convert_var<variable>   (variable &var) { return std::move(var); }
template<> std::string  convert_var<std::string>(variable &var) { return std::move(var.str()); }
template<> fixed_point  convert_var<fixed_point>(variable &var) { return std::move(var.number()); }

template<> int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> float        convert_var<float>      (variable &var) { return var.number().getAsDouble(); }
template<> double       convert_var<double>     (variable &var) { return var.number().getAsDouble(); }
template<> bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_variable = requires(variable v) {
    convert_var<std::decay_t<T>>(v);
};

template<typename T> struct is_optional_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_optional_impl<std::optional<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_optional = is_optional_impl<std::decay_t<T>>::value;

template<typename T> struct is_vector_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_vector_impl<std::vector<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_vector = is_vector_impl<std::decay_t<T>>::value;

template<typename ... Ts> struct count_required_impl {};
template<> struct count_required_impl<> {
    static constexpr size_t value = 0;
};
template<typename First, typename ... Ts> struct count_required_impl<First, Ts ...> {
    static constexpr size_t value = is_variable<First> ? 1 + count_required_impl<Ts ...>::value : 0;
};
template<typename ... Ts> constexpr size_t count_required = count_required_impl<Ts ...>::value;

template<bool Opt, typename ... Ts> struct verify_args_impl {};
template<bool Opt> struct verify_args_impl<Opt> {
    static constexpr bool value = true;
};
template<bool Opt, typename First, typename ... Ts>
struct verify_args_impl<Opt, First, Ts...> {
    static constexpr bool value = is_variable<First> ? (!Opt && verify_args_impl<false, Ts...>::value)
        : is_optional<First> ? verify_args_impl<true, Ts...>::value
            : is_vector<First> && sizeof...(Ts) == 0;
};
template<typename ... Ts> constexpr bool verify_args = verify_args_impl<false, Ts ...>::value;

template<typename ... Ts> struct has_varargs_impl {};
template<> struct has_varargs_impl<> {
    static constexpr bool value = false;
};
template<typename First, typename ... Ts> struct has_varargs_impl<First, Ts ...> {
    static constexpr bool value = is_vector<First> || has_varargs_impl<Ts ...>::value;
};
template<typename ... Ts> constexpr bool has_varargs = has_varargs_impl<std::decay_t<Ts> ...>::value;

template<size_t N, typename ... Ts> struct arg_type_impl {
    using type = void;
};

template<size_t N, typename First, typename ... Ts> struct arg_type_impl<N, First, Ts...> {
    using type = std::conditional_t<N==0, First, typename arg_type_impl<N-1,Ts...>::type>;
};

template<typename ... Ts> struct type_list {
    static constexpr size_t size = sizeof...(Ts);
};

template<size_t N, typename TypeList> struct get_type_n_impl {};
template<size_t N, typename ... Ts> struct get_type_n_impl<N, type_list<Ts ...>> {
    using type = typename arg_type_impl<N, Ts ...>::type;
};

template<size_t N, typename TypeList> using get_type_n = typename get_type_n_impl<N, TypeList>::type;

template<typename TypeList, size_t I> get_type_n<I, TypeList> get_arg(arg_list &args) {
    using type = get_type_n<I, TypeList>;
    if constexpr (is_optional<type>) {
        if (I >= args.size()) {
            return std::nullopt;
        } else {
            return convert_var<typename std::decay_t<type>::value_type>(args[I]);
        }
    } else if constexpr (is_vector<type>) {
        using vararg_type = typename std::decay_t<type>::value_type;
        std::vector<vararg_type> varargs;
        std::transform(
            args.begin() + I, args.end(), std::back_inserter(varargs),
            [](variable &var) {
                return convert_var<vararg_type>(var);
            }
        );
        return varargs;
    } else {
        return convert_var<type>(args[I]);
    }
}

template<typename TypeList, typename Function, std::size_t ... Is>
constexpr variable exec_helper(Function &fun, arg_list &args, std::index_sequence<Is...>) {
    return fun(get_arg<TypeList, Is>(args)...);
}

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

using function_handler = std::function<variable(arg_list &)>;

template<typename Function> struct check_args{};

template<typename T, typename ... Ts> requires(verify_args<Ts...>)
struct check_args<T (*)(Ts ...)>  {
    static constexpr size_t minargs = count_required<Ts...>;
    static constexpr size_t maxargs = has_varargs<Ts...> ? (size_t) -1 : sizeof...(Ts);
    using types = type_list<Ts...>;
};

template<typename Function>
constexpr std::pair<std::string, function_handler> create_function(const std::string &name, Function fun) {
    using fun_args = check_args<decltype(+fun)>;
    return {name, [fun](arg_list &vars) {
        if (vars.size() < fun_args::minargs || vars.size() > fun_args::maxargs) {
            throw invalid_numargs{vars.size(), fun_args::minargs, fun_args::maxargs};
        }
        return exec_helper<typename fun_args::types>(fun, vars,
            std::make_index_sequence<fun_args::types::size>{});
    }};
}

static const std::map<std::string, function_handler> dispatcher {
    create_function("search", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    create_function("searchall", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return string_join(search_regex_all(regex, str, index.value_or(1)), "\n");
    }),
    create_function("date", [](const std::string &str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("month", [](const std::string &str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_month(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("replace", [](std::string value, const std::string &regex, const std::string &to) {
        return string_replace_regex(value, regex, to);
    }),
    create_function("date_format", [](const std::string &month, const std::string &format) {
        return date_format(month, format);
    }),
    create_function("month_add", [](const std::string &month, int num) {
        return date_month_add(month, num);
    }),
    create_function("nonewline", [](const std::string &str) {
        return nonewline(str);
    }),
    create_function("if", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }),
    create_function("ifnot", [](bool condition, const variable &var_if, std::optional<variable> var_else) {
        return condition ? var_else.value_or(variable::null_var()) : var_if;
    }),
    create_function("contains", [](const std::string &str, const std::string &str2) {
        return str.find(str2) != std::string::npos;
    }),
    create_function("substr", [](const std::string &str, int pos, std::optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(str.substr(pos, count.value_or(std::string::npos)));
        }
        return variable::null_var();
    }),
    create_function("strlen", [](const std::string &str) {
        return str.size();
    }),
    create_function("strfind", [](const std::string &str, const std::string &value, std::optional<int> index) {
        return string_tolower(str).find(string_tolower(value), index.value_or(0));
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
    create_function("format", [](std::string format, std::vector<std::string> args) {
        for (size_t i=0; i<args.size(); ++i) {
            string_replace(format, fmt::format("${}", i), args[i]);
        }
        return format;
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
    })
};

void reader::call_function(const std::string &name, size_t numargs) {
    try {
        auto it = dispatcher.find(name);
        if (it != dispatcher.end()) {
            arg_list vars(numargs);
            for (size_t i=0; i<numargs; ++i) {
                vars[numargs - i - 1] = std::move(m_var_stack.top());
                m_var_stack.pop();
            }
            m_var_stack.push(it->second(vars));
        } else {
            throw layout_error(fmt::format("Funzione sconosciuta: {0}", name));
        }
    } catch (const invalid_numargs &error) {
        if (error.maxargs == (size_t) -1) {
            throw layout_error(fmt::format("La funzione {0} richiede minimo {1} argomenti", name, error.minargs));
        } else if (error.minargs == error.maxargs) {
            throw layout_error(fmt::format("La funzione {0} richiede {1} argomenti", name, error.minargs));
        } else {
            throw layout_error(fmt::format("La funzione {0} richiede {1}-{2} argomenti", name, error.minargs, error.maxargs));
        }
    }
}