#include "reader.h"

#include <functional>
#include <regex>
#include "utils.h"

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
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

template<size_t Minargs, size_t Maxargs, typename Function>
function_handler create_function(Function fun) {
    return [fun](const arg_list &vars) {
        if (vars.size() < Minargs || vars.size() > Maxargs) {
            throw invalid_numargs{vars.size(), Minargs, Maxargs};
        }
        return exec_helper(fun, vars, std::make_index_sequence<Maxargs>{});
    };
}

template<size_t Argnum, typename Function>
inline function_handler create_function(Function fun) {
    return create_function<Argnum, Argnum>(fun);
}

template<typename Function>
inline function_handler create_function(Function fun) {
    return create_function<1>(fun);
}

void reader::call_function(const std::string &name, size_t numargs) {
    static const std::map<std::string, function_handler> dispatcher {
        {"search", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            try {
                return search_regex(regex.str(), str.str(), index.empty() ? 1 : index.as_int());
            } catch (const std::regex_error &error) {
                throw layout_error(fmt::format("Espressione regolare non valida: {0}", regex.str()));
            }
        })},
        {"searchall", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            try {
                return string_join(search_regex_all(regex.str(), str.str(), index.empty() ? 1 : index.as_int()), "\n");
            } catch (const std::regex_error &error) {
                throw layout_error(fmt::format("Espressione regolare non valida: {0}", regex.str()));
            }
        })},
        {"date", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            try {
                return parse_date(regex.str(), str.str(), index.empty() ? 1 : index.as_int());
            } catch (const std::regex_error &error) {
                throw layout_error(fmt::format("Espressione regolare non valida: {0}", regex.str()));
            }
        })},
        {"format", [](const arg_list &args) {
            if (args.size() < 1) {
                throw layout_error("La funzione format richiede almeno 1 argomento");
            }
            std::vector<std::string> fmt_args(args.size()-1);
            std::transform(args.begin() + 1, args.end(), fmt_args.begin(), [](const variable &var) { return var.str(); });
            return string_format(args.front().str(), fmt_args);
        }},
        {"date_format", create_function<2>([](const variable &month, const variable &format) {
            return date_format(month.str(), format.str());
        })},
        {"month_add", create_function<2>([](const variable &month, const variable &num) { return date_month_add(month.str(), num.as_int()); })},
        {"nospace", create_function([](const variable &str) { return nospace(str.str()); })},
        {"ifl", create_function<2, 3>   ([](const variable &condition, const variable &var_if, const variable &var_else) { return condition.as_bool() ? var_if : var_else; })},
        {"coalesce", [](const arg_list &args) {
            for (auto &arg : args) {
                if (!arg.empty()) return arg;
            }
            return variable::null_var();
        }},
        {"contains", create_function<2>([](const variable &str, const variable &str2) {
            return str.str().find(str2.str()) != std::string::npos;
        })},
        {"substr", create_function<2, 3>([](const variable &str, const variable &pos, const variable &count) {
            if ((size_t) pos.as_int() < str.str().size()) {
                return variable(str.str().substr(pos.as_int(), count.empty() ? std::string::npos : count.as_int()));
            }
            return variable::null_var();
        })},
        {"strlen", create_function([](const variable &str) { return (int) str.str().size(); })},
        {"isempty", create_function([](const variable &str) { return str.empty(); })},
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