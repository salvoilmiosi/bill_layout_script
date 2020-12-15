#include "reader.h"

#include <functional>
#include <regex>
#include <fmt/format.h>

#include "functions.h"
#include "utils.h"

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

struct opt_variable : public variable {
    opt_variable(const variable &var) : variable(var) {}
};

template<typename ... Ts> struct count_required {};

template<typename ... Ts> constexpr size_t count_required_v = count_required<Ts...>::value;

template<typename T>
struct count_required<T> {
    static constexpr size_t value = ! std::is_base_of_v<opt_variable, T>;
};

template<typename First, typename ... Ts>
struct count_required<First, Ts...> {
    static constexpr size_t value = count_required_v<First> + count_required_v<Ts ...>;
};

using arg_list = std::vector<variable>;
using function_handler = std::function<variable(const arg_list &)>;

template<typename Function, std::size_t ... Is>
variable exec_helper(Function fun, const arg_list &args, std::index_sequence<Is...>) {
    auto get_arg = [](const arg_list &args, size_t index) -> const variable & {
        if (args.size() <= index) {
            return variable::null_var();
        } else {
            return args[index];
        }
    };
    return fun(get_arg(args, Is)...);
}

template<typename T, typename ... Ts>
function_handler create_function(T (*fun)(const Ts & ...)) {
    static constexpr size_t minargs = count_required_v<Ts...>;
    static constexpr size_t maxargs = sizeof...(Ts);
    return [fun](const arg_list &vars) {
        if (vars.size() < minargs || vars.size() > maxargs) {
            throw invalid_numargs{vars.size(), minargs, maxargs};
        }
        return exec_helper(fun, vars, std::make_index_sequence<maxargs>{});
    };
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
        {"format", [](const arg_list &args) {
            if (args.size() < 1) {
                throw layout_error("La funzione format richiede almeno 1 argomento");
            }
            std::vector<std::string> fmt_args;
            std::transform(std::next(args.begin()), args.end(),
                std::back_inserter(fmt_args),
                [](const variable &var) { return var.str(); });
            return string_format(args.front().str(), fmt_args);
        }},
        {"coalesce", [](const arg_list &args) {
            for (auto &arg : args) {
                if (!arg.empty()) return arg;
            }
            return variable::null_var();
        }},
        {"strcat", [](const arg_list &args) {
            std::string var;
            for (auto &arg : args) {
                var += arg.str();
            }
            return var;
        }}
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
        if (error.minargs == error.maxargs) {
            throw layout_error(fmt::format("La funzione {0} richiede {1} argomenti", name, error.minargs));
        } else {
            throw layout_error(fmt::format("La funzione {0} richiede {1}-{2} argomenti", name, error.minargs, error.maxargs));
        }
    }
}