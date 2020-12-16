#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

using var_list = std::vector<variable>;

template<typename T> struct is_variable_impl {
    static constexpr bool value = std::is_convertible_v<variable, std::remove_reference_t<T>>;
};
template<typename T> struct is_variable_impl<std::optional<T>> {
    static constexpr bool value = false;
};
template<typename T> constexpr bool is_variable = is_variable_impl<T>::value;

template<typename T> struct is_opt_impl {
    static constexpr bool value = false;
};
template<typename T> struct is_opt_impl<std::optional<T>> {
    static constexpr bool value = is_variable<T>;
};
template<typename T> constexpr bool is_opt = is_opt_impl<T>::value;

template<typename T> constexpr bool is_list = std::is_convertible_v<std::remove_reference_t<T>, var_list>;

template<typename ... Ts> struct count_required_impl {};
template<typename ... Ts> constexpr size_t count_required = count_required_impl<Ts ...>::value;

template<> struct count_required_impl<> {
    static constexpr size_t value = 0;
};

template<typename First, typename ... Ts> struct count_required_impl<First, Ts ...> {
    static constexpr size_t value = is_variable<First> + count_required<Ts ...>;
};

template<typename ... Ts> struct has_varargs_impl {};
template<typename ... Ts> constexpr bool has_varargs = has_varargs_impl<Ts ...>::value;

template<> struct has_varargs_impl<> {
    static constexpr bool value = false;
};

template<typename First, typename ... Ts> struct has_varargs_impl<First, Ts ...> {
    static constexpr size_t value = is_list<First> || has_varargs<Ts ...>;
};

template<bool Opt, typename ... Ts> struct verify_args_impl {};

template<bool Opt> struct verify_args_impl<Opt> {
    static constexpr bool value = true;
};

template<bool Opt, typename First, typename ... Ts>
struct verify_args_impl<Opt, First, Ts...> {
    static constexpr bool value = is_variable<First> ? (!Opt && verify_args_impl<false, Ts...>::value)
        : is_opt<First> ? verify_args_impl<true, Ts...>::value
            : is_list<First> && sizeof...(Ts) == 0;
};

template<typename ... Ts> constexpr bool verify_args = verify_args_impl<false, Ts ...>::value;

inline std::optional<variable> get_opt(var_list &args, size_t index) {
    if (args.size() <= index) {
        return std::nullopt;
    } else {
        return std::move(args[index]);
    }
}

template<typename Function, std::size_t ... Is, std::size_t ... Os>
constexpr variable exec_helper(Function fun, var_list &args, std::index_sequence<Is...>, std::index_sequence<Os...>) {
    return fun(std::move(args[Is])..., get_opt(args, Os)...);
}

template<typename Function, std::size_t ... Is, std::size_t ... Os>
constexpr variable exec_helper_varargs(Function fun, var_list &args, std::index_sequence<Is...>, std::index_sequence<Os...>) {
    return fun(std::move(args[Is])..., get_opt(args, Os)..., var_list(
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

using function_handler = std::function<variable(var_list &&)>;

template<typename T, typename ... Ts>
constexpr function_handler create_function(T (*fun)(Ts ...)) {
    static_assert(verify_args<Ts...>);

    constexpr size_t minargs = count_required<Ts...>;
    constexpr size_t maxargs = sizeof...(Ts);
    return [fun](var_list &&vars) {
        if constexpr (has_varargs<Ts...>) {
            if (vars.size() < minargs) {
                throw invalid_numargs{vars.size(), minargs, (size_t) -1};
            }
            return exec_helper_varargs(fun, vars, std::make_index_sequence<minargs>{}, make_offset_sequence<minargs, maxargs - 1>{});
        } else {
            if (vars.size() < minargs || vars.size() > maxargs) {
                throw invalid_numargs{vars.size(), minargs, maxargs};
            }
            return exec_helper(fun, vars, std::make_index_sequence<minargs>{}, make_offset_sequence<minargs, maxargs>{});
        }
    };
}

template<typename T>
constexpr function_handler create_function(T (*fun)(const var_list &args)) {
    return fun;
}

template<typename Function>
constexpr std::pair<std::string, function_handler> function_pair(const std::string &name, Function fun) {
    return {name, create_function(+fun)};
}

static const std::map<std::string, function_handler> dispatcher {
    function_pair("search", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return search_regex(regex, str, index.value_or(1));
    }),
    function_pair("searchall", [](const std::string &str, const std::string &regex, std::optional<int> index) {
        return string_join(search_regex_all(regex, str, index.value_or(1)), "\n");
    }),
    function_pair("date", [](const std::string &str, const std::string &format, const std::optional<std::string> &regex, std::optional<int> index) {
        return parse_date(format, str, regex.value_or(""), index.value_or(1));
    }),
    function_pair("month", [](const std::string &str, const std::string &format, const std::optional<std::string> &regex, std::optional<int> index) {
        return parse_month(format, str, regex.value_or(""), index.value_or(1));
    }),
    function_pair("replace", [](std::string value, const std::string &regex, const std::string &to) {
        string_replace_regex(value, regex, to);
        return value;
    }),
    function_pair("date_format", [](const std::string &month, const std::string &format) {
        return date_format(month, format);
    }),
    function_pair("month_add", [](const std::string &month, int num) {
        return date_month_add(month, num);
    }),
    function_pair("nonewline", [](const std::string &str) {
        return nonewline(str);
    }),
    function_pair("if", [](bool condition, const variable &var_if, const std::optional<variable> &var_else) {
        return condition ? var_if : var_else.value_or(variable::null_var());
    }),
    function_pair("ifnot", [](bool condition, const variable &var_if, const std::optional<variable> &var_else) {
        return condition ? var_else.value_or(variable::null_var()) : var_if;
    }),
    function_pair("contains", [](const std::string &str, const std::string &str2) {
        return str.find(str2) != std::string::npos;
    }),
    function_pair("substr", [](const std::string &str, int pos, std::optional<int> count) {
        if ((size_t) pos < str.size()) {
            return variable(str.substr(pos, count.value_or(std::string::npos)));
        }
        return variable::null_var();
    }),
    function_pair("strlen", [](const std::string &str) {
        return str.size();
    }),
    function_pair("strfind", [](const std::string &str, const std::string &value, std::optional<int> index) {
        return string_tolower(str).find(string_tolower(value), index.value_or(0));
    }),
    function_pair("isempty", [](const variable &str) {
        return str.empty();
    }),
    function_pair("percent", [](const std::string &str) {
        if (!str.empty()) {
            return variable(str + "%");
        } else {
            return variable::null_var();
        }
    }),
    function_pair("format", [](std::string format, const var_list &args) {
        for (size_t i=0; i<args.size(); ++i) {
            string_replace(format, fmt::format("${}", i), args[i].str());
        }
        return format;
    }),
    function_pair("coalesce", [](const var_list &args) {
        for (auto &arg : args) {
            if (!arg.empty()) return arg;
        }
        return variable::null_var();
    }),
    function_pair("strcat", [](const var_list &args) {
        std::string var;
        for (auto &arg : args) {
            var += arg.str();
        }
        return var;
    })
};

void reader::call_function(const std::string &name, size_t numargs) {
    try {
        auto it = dispatcher.find(name);
        if (it != dispatcher.end()) {
            var_list vars(numargs);
            for (size_t i=0; i<numargs; ++i) {
                vars[numargs - i - 1] = std::move(m_var_stack.top());
                m_var_stack.pop();
            }
            m_var_stack.push(it->second(std::move(vars)));
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