#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

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

template<typename ... Ts> class type_list {
private:
    template<size_t N, typename ... Rs> struct get_impl {
        using type = void;
    };

    template<size_t N, typename First, typename ... Rs> struct get_impl<N, First, Rs ...> {
        using type = std::conditional_t<N==0, First, typename get_impl<N-1, Rs...>::type>;
    };

public:
    static constexpr size_t size = sizeof...(Ts);
    template<size_t I> using get = typename get_impl<I, Ts ...>::type;
};

template<bool Req, typename ... Ts> struct check_args_impl {};
template<bool Req> struct check_args_impl<Req> {
    static constexpr bool minargs = 0;
    static constexpr bool maxargs = 0;

    static constexpr bool valid = true;
};

template<bool Req, typename First, typename ... Ts>
class check_args_impl<Req, First, Ts...> {
private:
    static constexpr bool var = is_variable<First>;
    static constexpr bool opt = is_optional<First>;
    static constexpr bool vec = is_vector<First>;

    using recursive = check_args_impl<Req ? var : !opt, Ts...>;

public:
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : 0;
    static constexpr size_t maxargs = (vec || recursive::maxargs == -1) ? -1 : 1 + recursive::maxargs;

    static constexpr bool valid = vec ? sizeof...(Ts) == 0 : (Req || opt) && recursive::valid;
};
template<typename Function> struct check_args {};
template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : public check_args_impl<true, Ts...> {
    using types = type_list<Ts ...>;
};

using arg_list = std::vector<variable>;
using function_handler = std::function<variable(arg_list &)>;

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

template<typename TypeList, size_t I> typename TypeList::get<I> get_arg(arg_list &args) {
    using type = typename TypeList::get<I>;
    if constexpr (is_optional<type>) {
        using opt_type = typename std::decay_t<type>::value_type;
        if (I >= args.size()) {
            return std::nullopt;
        } else {
            return convert_var<opt_type>(args[I]);
        }
    } else if constexpr (is_vector<type>) {
        using vec_type = typename std::decay_t<type>::value_type;
        if constexpr (std::is_same_v<vec_type, variable> && I==0) {
            return std::move(args);
        } else {
            std::vector<vec_type> varargs;
            std::transform(
                args.begin() + I, args.end(), std::back_inserter(varargs),
                [](variable &var) {
                    return convert_var<vec_type>(var);
                }
            );
            return varargs;
        }
    } else {
        return convert_var<type>(args[I]);
    }
}

template<typename TypeList, typename Function, std::size_t ... Is>
constexpr variable exec_helper(Function &fun, arg_list &args, std::index_sequence<Is...>) {
    return fun(get_arg<TypeList, Is>(args)...);
}

template<typename Function>
constexpr std::pair<std::string, function_handler> create_function(const std::string &name, Function fun) {
    using fun_args = check_args<decltype(+fun)>;
    static_assert(fun_args::valid);
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
            throw layout_error(fmt::format("La funzione {0} richiede almeno {1} argomenti", name, error.minargs));
        } else if (error.minargs == error.maxargs) {
            throw layout_error(fmt::format("La funzione {0} richiede {1} argomenti", name, error.minargs));
        } else {
            throw layout_error(fmt::format("La funzione {0} richiede {1}-{2} argomenti", name, error.minargs, error.maxargs));
        }
    }
}