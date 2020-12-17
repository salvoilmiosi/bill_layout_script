#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

using arg_list = std::vector<variable>;

template<typename T> struct is_variable_impl {
    static constexpr bool value = std::is_convertible_v<variable, T>;
};
template<typename T> struct is_variable_impl<std::optional<T>> {
    static constexpr bool value = false;
};
template<typename T> constexpr bool is_variable = is_variable_impl<std::decay_t<T>>::value;

template<typename T> struct is_opt_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_opt_impl<std::optional<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_opt = is_opt_impl<std::decay_t<T>>::value;

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
    static constexpr size_t value = is_variable<First> + count_required_impl<Ts ...>::value;
};
template<typename ... Ts> constexpr size_t count_required = count_required_impl<Ts ...>::value;

template<bool Opt, typename ... Ts> struct verify_args_impl {};
template<bool Opt> struct verify_args_impl<Opt> {
    static constexpr bool value = true;
};
template<bool Opt, typename First, typename ... Ts>
struct verify_args_impl<Opt, First, Ts...> {
    static constexpr bool value = is_variable<First> ? (!Opt && verify_args_impl<false, Ts...>::value)
        : is_opt<First> ? verify_args_impl<true, Ts...>::value
            : is_vector<First> && sizeof...(Ts) == 0;
};
template<typename ... Ts> constexpr bool verify_args = verify_args_impl<false, Ts ...>::value;

template<typename ... Ts> struct vararg_type_impl {
    using type = void;
};
template<typename T> struct vararg_type_impl<std::vector<T>> {
    using type = T;
};
template<typename First, typename ... Ts> struct vararg_type_impl<First, Ts ...> {
    using type = typename vararg_type_impl<Ts...>::type;
};
template<typename ... Ts> using get_varargs_type = typename vararg_type_impl<std::decay_t<Ts> ...>::type;

inline std::optional<variable> get_opt(arg_list &args, size_t index) {
    if (args.size() <= index) {
        return std::nullopt;
    } else {
        return std::move(args[index]);
    }
}

template<typename Function, std::size_t ... Is, std::size_t ... Os>
constexpr variable exec_helper(Function &fun, arg_list &args, std::index_sequence<Is...>, std::index_sequence<Os...>) {
    return fun(std::move(args[Is])..., get_opt(args, Os)...);
}

template<typename VarT, typename Function, std::size_t ... Is, std::size_t ... Os>
constexpr variable exec_helper_varargs(Function &fun, arg_list &args, std::index_sequence<Is...>, std::index_sequence<Os...>) {
    return fun(std::move(args[Is])..., get_opt(args, Os)..., std::vector<VarT>(
        std::make_move_iterator(args.begin() + sizeof...(Is) + sizeof...(Os)),
        std::make_move_iterator(args.end())
    ));
}

template<size_t Offset, typename T> struct offset_sequence_impl {};

template<size_t Offset, size_t ... Is>
struct offset_sequence_impl<Offset, std::index_sequence<Is ...>> {
    using type = std::index_sequence<Offset + Is...>;
};

template<size_t From, size_t To>
using make_offset_sequence = typename offset_sequence_impl<From, std::make_index_sequence<To - From>>::type;

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

using function_handler = std::function<variable(arg_list &)>;

template<typename Function, typename = void> struct check_args{};

template<typename T, typename ... Ts>
struct check_args<T (*)(Ts ...), std::enable_if_t<verify_args<Ts ...>>> {
    static constexpr size_t minargs = count_required<Ts...>;
    static constexpr size_t maxargs = sizeof...(Ts);
    using varargs_type = get_varargs_type<Ts...>;
};

template<typename Function>
constexpr std::pair<std::string, function_handler> create_function(const std::string &name, Function fun) {
    using fun_args = check_args<decltype(+fun)>;
    return {name, [fun](arg_list &vars) {
        if constexpr (!std::is_void_v<typename fun_args::varargs_type>) {
            if (vars.size() < fun_args::minargs) {
                throw invalid_numargs{vars.size(), fun_args::minargs, (size_t) -1};
            }
            return exec_helper_varargs<typename fun_args::varargs_type>(fun, vars,
                std::make_index_sequence<fun_args::minargs>{},
                make_offset_sequence<fun_args::minargs, fun_args::maxargs - 1>{});
        } else {
            if (vars.size() < fun_args::minargs || vars.size() > fun_args::maxargs) {
                throw invalid_numargs{vars.size(), fun_args::minargs, fun_args::maxargs};
            }
            return exec_helper(fun, vars,
                std::make_index_sequence<fun_args::minargs>{},
                make_offset_sequence<fun_args::minargs, fun_args::maxargs>{});
        }
    }};
}

static const std::map<std::string, function_handler> dispatcher {
    create_function("search", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    create_function("searchall", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return string_join(search_regex_all(regex, str, index.value_or(1)), "\n");
    }),
    create_function("date", [](const std::string &str, const std::string &format, const std::optional<std::string> &regex, std::optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    create_function("month", [](const std::string &str, const std::string &format, const std::optional<std::string> &regex, std::optional<int> index) {
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
    create_function("if", [](bool condition, const variable &var_if, const std::optional<variable> &var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }),
    create_function("ifnot", [](bool condition, const variable &var_if, const std::optional<variable> &var_else) {
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
    create_function("isempty", [](const variable &str) {
        return str.empty();
    }),
    create_function("percent", [](const std::string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable::null_var();
        }
    }),
    create_function("format", [](std::string format, const std::vector<std::string> &args) {
        for (size_t i=0; i<args.size(); ++i) {
            string_replace(format, fmt::format("${}", i), args[i]);
        }
        return format;
    }),
    create_function("coalesce", [](const arg_list &args) {
        for (auto &arg : args) {
            if (!arg.empty()) return arg;
        }
        return variable::null_var();
    }),
    create_function("strcat", [](const std::vector<std::string> &args) {
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