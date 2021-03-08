#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <functional>
#include <ranges>
#include <span>

#include "variable.h"
#include "lexer.h"

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &&convert_var<variable &&>    (variable &var) { return std::move(var); }
template<> inline std::string &&convert_var<std::string &&> (variable &var) { return std::move(var).str(); }

template<> inline std::string_view  convert_var<std::string_view> (variable &var) { return var.str_view(); }
template<> inline fixed_point  convert_var<fixed_point> (variable &var) { return var.number(); }
template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.as_double(); }
template<> inline double       convert_var<double>     (variable &var) { return var.as_double(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_convertible = requires(variable &v) {
    convert_var<T>(v);
};

template<typename T> using convert_type = std::conditional_t<
    is_convertible<std::add_rvalue_reference_t<std::decay_t<T>>>,
        std::add_rvalue_reference_t<std::decay_t<T>>,
        std::decay_t<T>>;

template<typename T>
struct converter {
    using out_type = convert_type<T>;
    out_type operator ()(variable &var) {
        return convert_var<out_type>(var);
    }
};

using arg_list = std::span<variable>;

template<typename T> using varargs_base = std::ranges::transform_view<arg_list, converter<T>>;
template<typename T> struct varargs : varargs_base<T> {
    using var_type = T;
    varargs(auto &&obj) : varargs_base<T>(std::forward<decltype(obj)>(obj), converter<T>{}) {}
    bool empty() {
        return varargs_base<T>::size() == 0;
    }
};

class invalid_numargs : public parsing_error {
private:
    static std::string get_message(const std::string &fun_name, size_t minargs, size_t maxargs) {
        if (maxargs == std::numeric_limits<size_t>::max()) {
            return fmt::format("La funzione {0} richiede almeno {1} argomenti", fun_name, minargs);
        } else if (minargs == maxargs) {
            return fmt::format("La funzione {0} richiede {1} argomenti", fun_name, minargs);
        } else {
            return fmt::format("La funzione {0} richiede {1}-{2} argomenti", fun_name, minargs, maxargs);
        }
    }

public:
    invalid_numargs(const std::string &fun_name, size_t minargs, size_t maxargs, token &tok)
        : parsing_error(get_message(fun_name, minargs, maxargs), tok) {}
};

class function_handler {
public:
    size_t minargs;
    size_t maxargs;

    template<typename Function> function_handler(Function fun);

    variable operator ()(auto &&args) const {
        return m_fun(std::forward<decltype(args)>(args));
    }

private:
    std::function<variable(arg_list&&)> m_fun;
};

const function_handler &find_function(const std::string &name);

#endif