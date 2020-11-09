#include "reader.h"

#include <functional>
#include <fmt/format.h>
#include "../shared/utils.h"

struct invalid_numargs {
    size_t numargs;
    size_t minargs;
    size_t maxargs;
};

struct scripted_error {
    const std::string &msg;
};

using arg_list = std::vector<variable>;
using function_handler = std::function<variable(const arg_list &)>;

template<typename Function, std::size_t ... Is>
variable exec_helper(Function fun, const arg_list &args, std::index_sequence<Is...>) {
    auto get_arg = [](const arg_list &args, size_t index) -> const variable & {
        static const variable VAR_NULL;
        if (args.size() <= index) {
            return VAR_NULL;
        } else {
            return args[index];
        }
    };
    return fun(get_arg(args, Is)...);
}

template<size_t Minargs, size_t Maxargs, typename Function>
function_handler create_function(Function fun) {
    return [&](const arg_list &vars) {
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
        {"eq",      create_function<2>([](const variable &a, const variable &b) { return a == b; })},
        {"neq",     create_function<2>([](const variable &a, const variable &b) { return a != b; })},
        {"and",     create_function<2>([](const variable &a, const variable &b) { return a && b; })},
        {"or",      create_function<2>([](const variable &a, const variable &b) { return a || b; })},
        {"not",     create_function   ([](const variable &var) { return !var; })},
        {"add",     create_function<2>([](const variable &a, const variable &b) { return a + b; })},
        {"sub",     create_function<2>([](const variable &a, const variable &b) { return a - b; })},
        {"mul",     create_function<2>([](const variable &a, const variable &b) { return a * b; })},
        {"div",     create_function<2>([](const variable &a, const variable &b) { return a / b; })},
        {"gt",      create_function<2>([](const variable &a, const variable &b) { return a > b; })},
        {"lt",      create_function<2>([](const variable &a, const variable &b) { return a < b; })},
        {"geq",     create_function<2>([](const variable &a, const variable &b) { return a >= b; })},
        {"leq",     create_function<2>([](const variable &a, const variable &b) { return a <= b; })},
        {"max",     create_function<2>([](const variable &a, const variable &b) { return std::max(a, b); })},
        {"min",     create_function<2>([](const variable &a, const variable &b) { return std::min(a, b); })},
        {"search", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            return search_regex(regex.str(), str.str(), index ? index.asInt() : 1);
        })},
        {"date", create_function<2, 3>([](const variable &str, const variable &regex, const variable &index) {
            return parse_date(regex.str(), str.str(), index ? index.asInt() : 1);
        })},
        {"month_begin", create_function([](const variable &str) { return str.str() + "-01"; })},
        {"month_end", create_function([](const variable &str) { return date_month_end(str.str()); })},
        {"nospace", create_function([](const variable &str) { return nospace(str.str()); })},
        {"num", create_function([](const variable &str) { return parse_number(str.str()); })},
        {"ifl", create_function<2, 3>   ([](const variable &condition, const variable &var_if, const variable &var_else) { return condition ? var_if : var_else; })},
        {"coalesce", [](const arg_list &args) {
            for (auto &arg : args) {
                if (arg) return arg;

            }
            return variable();
        }},
        {"contains", create_function<2>([](const variable &str, const variable &str2) {
            return str.str().find(str2.str()) != std::string::npos;
        })},
        {"substr", create_function<2, 3>([](const variable &str, const variable &pos, const variable &count) {
            if ((size_t) pos.asInt() < str.str().size()) {
                return variable(str.str().substr(pos.asInt(), count ? count.asInt() : std::string::npos));
            }
            return variable();
        })},
        {"strlen", create_function([](const variable &str) { return str.str().size(); })},
        {"isempty", create_function([](const variable &str) { return str.empty(); })},
        {"strcat", [](const arg_list &args) {
            std::string var;
            for (auto &arg : args) {
                var += arg.str();
            }
            return var;
        }},
        {"error", create_function([](const variable &msg) {
            if (msg) throw scripted_error{msg.str()};
            return variable();
        })},
        {"int", create_function([](const variable &str) {
            return variable(std::to_string(str.asInt()), VALUE_NUMBER);
            // converte da stringa a fixed_point a int a stringa ...
        })}
    };

    auto it = dispatcher.find(name);
    if (it != dispatcher.end()) {
        arg_list vars;
        for (size_t i=0; i<numargs; ++i) {
            vars.push_back(var_stack[var_stack.size()-numargs+i]);
        }
        var_stack.resize(var_stack.size() - numargs);
        var_stack.push_back(it->second(vars));
    } else {
        throw assembly_error{fmt::format("Funzione sconosciuta: {0}", name)};
    }
}