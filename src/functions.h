#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <ranges>
#include <map>

#include "variable.h"
#include "stack.h"
#include "utils.h"

template<typename T> T convert_var(variable &var) = delete;

template<> inline variable    &&convert_var<variable &&>    (variable &var) { return std::move(var); }
template<> inline std::string &&convert_var<std::string &&> (variable &var) { return std::move(var).as_string(); }

template<> inline const variable    &convert_var<const variable &>    (variable &var) { return var; }
template<> inline const std::string &convert_var<const std::string &> (variable &var) { return var.as_string(); }

template<> inline std::string_view  convert_var<std::string_view> (variable &var) { return var.as_view(); }
template<> inline fixed_point  convert_var<fixed_point> (variable &var) { return var.as_number(); }
template<> inline datetime     convert_var<datetime>    (variable &var) { return var.as_date(); }
template<> inline size_t       convert_var<size_t>     (variable &var) { return var.as_int(); }
template<> inline int          convert_var<int>        (variable &var) { return var.as_int(); }
template<> inline float        convert_var<float>      (variable &var) { return var.as_double(); }
template<> inline double       convert_var<double>     (variable &var) { return var.as_double(); }
template<> inline bool         convert_var<bool>       (variable &var) { return var.as_bool(); }

using string_list = decltype(string_split(std::declval<std::string_view>(), UNIT_SEPARATOR));

template<> inline string_list convert_var<string_list> (variable &var) { return string_split(var.as_view(), UNIT_SEPARATOR); }

template<typename T, typename = void> struct is_convertible_impl : std::false_type {};
template<typename T> struct is_convertible_impl<T, std::void_t<decltype(convert_var<T>(std::declval<variable &>()))>> : std::true_type {};
template<typename T> constexpr bool is_convertible = is_convertible_impl<T>::value;

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
    decltype(auto) operator ()(variable &var) const {
        return convert_var<convert_lvalue<T>>(var);
    }
};

using arg_list = std::ranges::subrange<typename simple_stack<variable>::iterator>;

// using decltype([]{ return T{}; }) faceva crashare gcc
template<typename T> struct getter {
    T operator ()() {
        return T{};
    }
};

template<std::integral T, T Value> struct int_getter {
    T operator ()() {
        return Value;
    }
};

template<typename T, typename DefaultGetter = getter<T>>
struct optional : T {
    using value_type = T;

    optional() : T(DefaultGetter{}()) {}

    template<typename U>
    optional(U &&value) : T(std::forward<U>(value)) {}
};

template<std::integral T, typename DefaultGetter>
struct optional<T, DefaultGetter> {
    using value_type = T;
    
    T m_value;

    optional() : m_value(DefaultGetter{}()) {}

    template<typename U>
    optional(U &&value) : m_value(std::forward<U>(value)) {}

    operator T() const {
        return m_value;
    }
};

template<int Value>     using optional_int =    optional<int,    int_getter<int, Value>>;
template<size_t Value>  using optional_size =   optional<size_t, int_getter<size_t, Value>>;

template<typename T> using varargs_base = std::ranges::transform_view<arg_list, vararg_converter<T>>;
template<typename T, size_t Minargs = 0> struct varargs : varargs_base<T> {
    using var_type = T;
    template<typename U>
    varargs(U &&obj) : varargs_base<T>(std::forward<U>(obj), vararg_converter<T>{}) {}
};

template<typename T> struct is_variable : std::bool_constant<! std::is_void_v<convert_rvalue<T>>> {};

template<typename T> struct is_optional_impl : std::false_type {};
template<typename T, typename Getter> struct is_optional_impl<optional<T, Getter>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_optional : is_optional_impl<std::decay_t<T>> {};

template<typename T> struct is_varargs_impl : std::false_type {};
template<typename T, size_t Minargs> struct is_varargs_impl<varargs<T, Minargs>> : std::bool_constant<is_variable<T>{}> {};
template<typename T> struct is_varargs : is_varargs_impl<std::decay_t<T>> {};

template<typename ... Ts> struct type_list {
    static constexpr size_t size = sizeof...(Ts);
};

template<size_t N, typename TypeList> struct get_nth {};

template<size_t N, typename First, typename ... Ts> struct get_nth<N, type_list<First, Ts...>> {
    using type = typename get_nth<N-1, type_list<Ts ...>>::type;
};

template<typename First, typename ... Ts> struct get_nth<0, type_list<First, Ts ...>> {
    using type = First;
};

template<size_t I, typename TypeList> using get_nth_t = typename get_nth<I, TypeList>::type;

template<bool Req, typename ... Ts> struct check_args_impl {};
template<bool Req> struct check_args_impl<Req> {
    static constexpr bool minargs = 0;
    static constexpr bool maxargs = 0;

    static constexpr bool valid = true;
};

template<typename T> struct vararg_minargs {
    static constexpr size_t value = 0;
};

template<typename T, size_t Minargs> struct vararg_minargs<varargs<T, Minargs>> {
    static constexpr size_t value = Minargs;
};

template<typename T> constexpr size_t vararg_minargs_v = vararg_minargs<T>::value;

template<bool Req, typename First, typename ... Ts>
class check_args_impl<Req, First, Ts ...> {
private:
    static constexpr bool var = is_variable<First>{};
    static constexpr bool opt = is_optional<First>{};
    static constexpr bool vec = is_varargs<First>{};

    // Req indica se il parametro precedente non era opt
    using recursive = check_args_impl<Req ? var : !opt, Ts ...>;

public:
    // Conta il numero di tipi che non sono opzionali
    static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : vec ? vararg_minargs_v<First> : recursive::minargs;
    // Conta il numero totale di tipi o INT_MAX se l'ultimo è varargs<T>
    static constexpr size_t maxargs = (vec || recursive::maxargs == std::numeric_limits<size_t>::max()) ?
        std::numeric_limits<size_t>::max() : 1 + recursive::maxargs;

    // true solo se i tipi degli argomenti seguono il pattern (var*N, opt*M, [vec])
    // Tutti gli argomenti opzionali devono essere dopo quelli richiesti
    static constexpr bool valid = vec ? sizeof...(Ts) == 0 && (Req || vararg_minargs_v<First> == 0) : (Req || opt) && recursive::valid;
};

template<typename Function> struct check_args {};
template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : check_args_impl<true, Ts ...> {
    static_assert(check_args_impl<true, Ts...>::valid, "Gli argomenti della funzione non sono validi");
};

template<typename T, typename ... Ts> struct function_types_impl {};
template<typename T, typename ... Ts> struct function_types_impl<T(*)(Ts ...)> {
    using type = type_list<Ts ...>;
};

template<typename T> using function_types = typename function_types_impl<T>::type;

template<typename TypeList, size_t I> inline decltype(auto) get_arg(arg_list &args) {
    using type = std::decay_t<get_nth_t<I, TypeList>>;
    if constexpr (is_optional<type>{}) {
        using opt_type = typename type::value_type;
        if (I >= args.size()) {
            return type();
        } else {
            return type(convert_var<convert_rvalue<opt_type>>(args[I]));
        }
    } else if constexpr (is_varargs<type>{}) {
        return type(arg_list(args.begin() + I, args.end()));
    } else {
        return convert_var<convert_rvalue<type>>(args[I]);
    }
}

class function_handler {
private:
    variable (*const m_fun) (arg_list &&);

    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template

    // function_handler rappresenta una closure che passa automaticamente gli argomenti
    // da arg_list alla funzione fun, convertendoli nei tipi giusti
    template<typename Function> static variable call_function(arg_list &&args) {
        using types = function_types<decltype(+Function{})>;
        return [] <size_t ... Is> (arg_list &args, std::index_sequence<Is...>) {
            return Function{}(get_arg<types, Is>(args) ...);
        }(args, std::make_index_sequence<types::size>{});
    }

public:
    const size_t minargs;
    const size_t maxargs;

    template<typename Function>
    function_handler(Function fun)
        : m_fun(call_function<Function>)
        , minargs(check_args<decltype(+fun)>::minargs)
        , maxargs(check_args<decltype(+fun)>::maxargs) {}

    variable operator ()(arg_list &&args) const {
        return m_fun(std::move(args));
    }
};

using function_map = std::map<std::string, function_handler, std::less<>>;
using function_iterator = function_map::const_iterator;

extern const function_map function_lookup;

#endif