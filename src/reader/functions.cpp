#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &convert_var<variable &>    (variable &var) { return var; }
template<> inline std::string &convert_var<std::string &> (variable &var) { return var.str(); }
template<> inline fixed_point &convert_var<fixed_point &> (variable &var) { return var.number(); }

template<> inline variable     convert_var<variable>   (variable &var) { return std::move(var); }
template<> inline std::string  convert_var<std::string>(variable &var) { return std::move(var.str()); }
template<> inline fixed_point  convert_var<fixed_point>(variable &var) { return std::move(var.number()); }

template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.number().getAsDouble(); }
template<> inline double       convert_var<double>     (variable &var) { return var.number().getAsDouble(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_variable = requires(variable v) {
    convert_var<T>(v);
};

template<typename T> struct is_optional_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_optional_impl<std::optional<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_optional = is_optional_impl<T>::value;

template<typename T> struct is_vector_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_vector_impl<std::vector<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_vector = is_vector_impl<T>::value;

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

template<bool Req, typename ... Ts> struct check_args_impl {};
template<bool Req> struct check_args_impl<Req> {
    static constexpr bool minargs = 0;
    static constexpr bool maxargs = 0;

    static constexpr bool valid = true;
};

template<bool Req, typename First, typename ... Ts>
class check_args_impl<Req, First, Ts ...> {
private:
    static constexpr bool var = is_variable<std::decay_t<First>>;
    static constexpr bool opt = is_optional<std::decay_t<First>>;
    static constexpr bool vec = is_vector<std::decay_t<First>>;

    using recursive = check_args_impl<Req ? var : !opt, Ts ...>;

public:
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : 0;
    static constexpr size_t maxargs = (vec || recursive::maxargs == -1) ? -1 : 1 + recursive::maxargs;

    static constexpr bool valid = vec ? sizeof...(Ts) == 0 : (Req || opt) && recursive::valid;
};
template<typename Function> struct check_args {};
template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : public check_args_impl<true, Ts ...> {
    using types = type_list<Ts ...>;
};

using arg_list = my_stack<variable>::range;

template<typename T> using convert_type = std::conditional_t<is_variable<T>,T,std::decay_t<T>>;

template<typename T> using convert_ref = convert_type<std::add_lvalue_reference_t<T>>;

template<typename T, typename InputIt, typename Function>
constexpr std::vector<T> transformed_vector(InputIt begin, InputIt end, Function fun) {
    std::vector<T> ret;
    for (;begin!=end; ++begin) {
        ret.emplace_back(std::move(fun(*begin)));
    }
    return ret;
}

template<typename TypeList, size_t I> inline convert_type<typename TypeList::get<I>> get_arg(arg_list &args) {
    using type = convert_type<typename TypeList::get<I>>;
    if constexpr (is_optional<type>) {
        using opt_type = typename type::value_type;
        if (I >= args.size()) {
            return std::nullopt;
        } else {
            return convert_var<opt_type>(args[I]);
        }
    } else if constexpr (is_vector<type>) {
        using vec_type = typename type::value_type;
        return transformed_vector<vec_type>(args.begin() + I, args.end(), convert_var<convert_ref<vec_type>>);
    } else {
        return convert_var<type>(args[I]);
    }
}

template<typename TypeList, typename Function, std::size_t ... Is>
constexpr variable exec_helper(Function fun, arg_list &args, std::index_sequence<Is ...>) {
    return fun(get_arg<TypeList, Is>(args)...);
}

void check_numargs(const std::string &name, size_t numargs, size_t minargs, size_t maxargs) {
    if (numargs < minargs || numargs > maxargs) {
        if (maxargs == (size_t) -1) {
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
constexpr std::pair<std::string, function_handler> create_function(const std::string &name, Function fun) {
    using fun_args = check_args<decltype(+fun)>;
    static_assert(fun_args::valid);
    return {name, [name, fun](arg_list &&args) {
        check_numargs(name, args.size(), fun_args::minargs, fun_args::maxargs);
        return exec_helper<typename fun_args::types>(fun, args,
            std::make_index_sequence<fun_args::types::size>{});
    }};
}

static const std::map<std::string, function_handler> lookup {
    create_function("search", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    create_function("searchall", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return string_join(search_regex_all(regex, str, index.value_or(1)), "\n");
    }),
    create_function("date", [](const std::string &str, const std::string &format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("month", [](const std::string &str, std::optional<std::string> format, std::optional<std::string> regex, std::optional<int> index) {
        return parse_month(format.value_or(""), str, regex.value_or(""), index.value_or(1));
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
        return str.find(value, index.value_or(0));
    }),
    create_function("tolower", [](const std::string &str) {
        return string_tolower(str);
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
    auto it = lookup.find(name);
    if (it != lookup.end()) {
        variable ret = it->second(m_var_stack.top_view(numargs));
        m_var_stack.resize(m_var_stack.size() - numargs);
        m_var_stack.push(std::move(ret));
    } else {
        throw layout_error(fmt::format("Funzione sconosciuta: {0}", name));
    }
}