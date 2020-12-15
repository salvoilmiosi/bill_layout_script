#include "reader.h"

#include <functional>
#include <regex>
#include <optional>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

struct opt_variable : public variable {
    opt_variable(const variable &var) : variable(var) {}
};

using arg_list = std::vector<variable>;

template<typename T> constexpr bool is_variable = std::is_same_v<T, variable>;
template<typename T> constexpr bool is_opt = std::is_same_v<T, opt_variable>;
template<typename T> constexpr bool is_list = std::is_same_v<T, arg_list>;

template<typename T, typename ... Ts> struct count_types {};
template<typename T, typename ... Ts> constexpr size_t count_types_v = count_types<T, Ts...>::value;

template<typename T> struct count_types<T> {
    static constexpr size_t value = 0;
};

template<typename T, typename First, typename ... Ts>
struct count_types<T, First, Ts...> {
    static constexpr size_t value = std::is_same_v<T, First> + count_types_v<T, Ts ...>;
};

template<typename ... Ts> constexpr size_t count_required = count_types_v<variable, Ts ...>;
template<typename ... Ts> constexpr bool has_varargs = count_types_v<arg_list, Ts ...> != 0;

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

inline const opt_variable &get_arg(const arg_list &args, size_t index) {
    if (args.size() <= index) {
        return variable::null_var();
    } else {
        return args[index];
    }
}

template<typename Function, std::size_t ... Is>
inline variable exec_helper(Function fun, const arg_list &args, std::index_sequence<Is...>) {
    return fun(get_arg(args, Is)...);
}

template<typename Function, std::size_t ... Is>
inline variable exec_helper_varargs(Function fun, const arg_list &args, std::index_sequence<Is...>) {
    return fun(get_arg(args, Is)..., arg_list(
        std::make_move_iterator(args.begin() + sizeof...(Is)),
        std::make_move_iterator(args.end())
    ));
}

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

using function_handler = std::function<variable(const arg_list &)>;

template<typename T, typename ... Ts>
function_handler create_function(T (*fun)(const Ts & ...)) {
    static_assert(verify_args<Ts...>);

    static constexpr size_t minargs = count_required<Ts...>;
    static constexpr size_t maxargs = sizeof...(Ts);
    return [fun](const arg_list &vars) {
        if constexpr (has_varargs<Ts...>) {
            if (vars.size() < minargs) {
                throw invalid_numargs{vars.size(), minargs, (size_t) -1};
            }
            return exec_helper_varargs(fun, vars, std::make_index_sequence<maxargs - 1>{});
        } else {
            if (vars.size() < minargs || vars.size() > maxargs) {
                throw invalid_numargs{vars.size(), minargs, maxargs};
            }
            return exec_helper(fun, vars, std::make_index_sequence<maxargs>{});
        }
    };
}

template<typename T>
function_handler create_function(T (*fun)(const arg_list &args)) {
    return fun;
}

template<typename Function>
inline std::pair<std::string, function_handler> function_pair(const std::string &name, Function fun) {
    return {name, create_function(+fun)};
}

void reader::call_function(const std::string &name, size_t numargs) {
    static const std::map<std::string, function_handler> dispatcher {
        function_pair("search", [](const variable &str, const variable &regex, const opt_variable &index) {
            return search_regex(regex.str(), str.str(), index.empty() ? 1 : index.as_int());
        }),
        function_pair("searchall", [](const variable &str, const variable &regex, const opt_variable &index) {
            return string_join(search_regex_all(regex.str(), str.str(), index.empty() ? 1 : index.as_int()), "\n");
        }),
        function_pair("date", [](const variable &str, const variable &format, const opt_variable &regex, const opt_variable &index) {
            return parse_date(format.str(), str.str(), regex.str(), index.empty() ? 1 : index.as_int());
        }),
        function_pair("month", [](const variable &str, const opt_variable &format, const opt_variable &regex, const opt_variable &index) {
            return parse_month(format.str(), str.str(), regex.str(), index.empty() ? 1 : index.as_int());
        }),
        function_pair("replace", [](const variable &value, const variable &regex, const variable &to) {
            std::string str = value.str();
            string_replace_regex(str, regex.str(), to.str());
            return str;
        }),
        function_pair("date_format", [](const variable &month, const variable &format) {
            return date_format(month.str(), format.str());
        }),
        function_pair("month_add", [](const variable &month, const variable &num) {
            return date_month_add(month.str(), num.as_int());
        }),
        function_pair("nonewline", [](const variable &str) {
            return nonewline(str.str());
        }),
        function_pair("if", [](const variable &condition, const variable &var_if, const opt_variable &var_else) {
            return condition.as_bool() ? var_if : var_else;
        }),
        function_pair("ifnot", [](const variable &condition, const variable &var_if, const opt_variable &var_else) {
            return condition.as_bool() ? var_else : var_if;
        }),
        function_pair("contains", [](const variable &str, const variable &str2) {
            return str.str().find(str2.str()) != std::string::npos;
        }),
        function_pair("substr", [](const variable &str, const variable &pos, const opt_variable &count) {
            if ((size_t) pos.as_int() < str.str().size()) {
                return variable(str.str().substr(pos.as_int(), count.empty() ? std::string::npos : count.as_int()));
            }
            return variable::null_var();
        }),
        function_pair("strlen", [](const variable &str) {
            return (int) str.str().size();
        }),
        function_pair("strfind", [](const variable &str, const variable &value, const opt_variable &index) {
            return (int) string_tolower(str.str()).find(string_tolower(value.str()), index.as_int());
        }),
        function_pair("isempty", [](const variable &str) {
            return str.empty();
        }),
        function_pair("percent", [](const variable &str) {
            if (!str.empty()) {
                return variable(str.str() + "%");
            } else {
                return variable::null_var();
            }
        }),
        function_pair("format", [](const variable &format, const arg_list &args) {
            std::string str = format.str();
            for (size_t i=0; i<args.size(); ++i) {
                string_replace(str, fmt::format("${}", i), args[i].str());
            }
            return str;
        }),
        function_pair("coalesce", [](const arg_list &args) {
            for (auto &arg : args) {
                if (!arg.empty()) return arg;
            }
            return variable::null_var();
        }),
        function_pair("strcat", [](const arg_list &args) {
            std::string var;
            for (auto &arg : args) {
                var += arg.str();
            }
            return var;
        })
    };

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