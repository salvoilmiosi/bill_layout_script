#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <functional>
#include <ranges>
#include <span>
#include <map>

#include "variable.h"
#include "lexer.h"

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &&convert_var<variable &&>    (variable &var) { return std::move(var); }
template<> inline std::string &&convert_var<std::string &&> (variable &var) { return std::move(var).str(); }

template<> inline const variable    &convert_var<const variable &>    (variable &var) { return var; }
template<> inline const std::string &convert_var<const std::string &> (variable &var) { return var.str(); }

template<> inline std::string_view  convert_var<std::string_view> (variable &var) { return var.str_view(); }
template<> inline fixed_point  convert_var<fixed_point> (variable &var) { return var.number(); }
template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.as_double(); }
template<> inline double       convert_var<double>     (variable &var) { return var.as_double(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

template<typename T> constexpr bool is_convertible = requires(variable &v) {
    convert_var<T>(v);
};

template<typename ... Ts> struct first_convertible {};
template<> struct first_convertible<> {
    using type = void;
};
template<typename First, typename ... Ts> struct first_convertible<First, Ts...> {
    using type = std::conditional_t<is_convertible<First>, First, typename first_convertible<Ts...>::type>;
};
template<typename ... Ts> using first_convertible_t = typename first_convertible<Ts...>::type;

template<typename T> using convert_rvalue = first_convertible_t<
    std::add_rvalue_reference_t<std::decay_t<T>>, std::decay_t<T>>;
template<typename T> using convert_lvalue = first_convertible_t<
    std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>>, std::decay_t<T>>;

template<typename T>
struct vararg_converter {
    using out_type = convert_lvalue<T>;
    out_type operator ()(variable &var) const {
        return convert_var<out_type>(var);
    }
};

using arg_list = std::span<variable>;

template<typename T> using varargs_base = std::ranges::transform_view<arg_list, vararg_converter<T>>;
template<typename T> struct varargs : varargs_base<T> {
    using var_type = T;
    varargs(auto &&obj) : varargs_base<T>(std::forward<decltype(obj)>(obj), vararg_converter<T>{}) {}
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

using function_base = std::function<variable(arg_list&&)>;
struct function_handler : function_base {
    size_t minargs;
    size_t maxargs;

    template<typename Function> function_handler(Function fun);
};

extern const std::map<std::string, function_handler> function_lookup;

using function_iterator = decltype(function_lookup)::const_iterator;

#endif