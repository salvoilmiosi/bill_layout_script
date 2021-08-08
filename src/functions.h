#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <ranges>
#include <map>

#include "variable.h"
#include "stack.h"
#include "utils.h"
#include "type_list.h"

namespace bls {

    template<typename T> struct variable_converter {};

    #define DEFINE_CONVERTER(t, cmd) template<> struct variable_converter<t> { t operator()(variable &var) { return cmd; } };

    DEFINE_CONVERTER(variable &&,           std::move(var))
    DEFINE_CONVERTER(std::string &&,        std::move(var).as_string())
    DEFINE_CONVERTER(const variable &,      var)
    DEFINE_CONVERTER(const std::string &,   var.as_string())
    DEFINE_CONVERTER(std::string_view,      var.as_view())
    DEFINE_CONVERTER(fixed_point,           var.as_number())
    DEFINE_CONVERTER(datetime,              var.as_date())
    DEFINE_CONVERTER(size_t,                var.as_int())
    DEFINE_CONVERTER(int,                   var.as_int())
    DEFINE_CONVERTER(float,                 var.as_double())
    DEFINE_CONVERTER(double,                var.as_double())
    DEFINE_CONVERTER(bool,                  var.as_bool())

    #undef DEFINE_CONVERTER

    template<typename T, typename = void> struct is_convertible : std::false_type {};
    template<typename T> struct is_convertible<T, std::void_t<decltype(variable_converter<T>{}(std::declval<variable&>()))>> : std::true_type {};
    template<typename T> constexpr bool is_convertible_v = is_convertible<T>::value;

    template<typename ... Ts> struct first_convertible {};
    template<typename ... Ts> using first_convertible_t = typename first_convertible<Ts...>::type;
    template<> struct first_convertible<> { using type = void; };
    template<typename First, typename ... Ts> struct first_convertible<First, Ts...>
        : std::conditional<is_convertible_v<First>, First, first_convertible_t<Ts...>> {};

    template<typename T> using convert_rvalue = first_convertible_t<
        std::add_rvalue_reference_t<std::decay_t<T>>, std::decay_t<T>>;
    template<typename T> using convert_lvalue = first_convertible_t<
        std::add_lvalue_reference_t<std::add_const_t<std::decay_t<T>>>, std::decay_t<T>>;

    template<typename T> struct variable_converter<std::vector<T>> {
        std::vector<T> operator()(variable &var) const {
            auto arr = std::move(var).as_array();
            auto view = arr | std::views::transform(
                variable_converter<convert_rvalue<T>>{});
            return {view.begin(), view.end()};
        }
    };

    using arg_list = std::ranges::subrange<typename simple_stack<variable>::iterator>;

    // using decltype([]{ return T{}; }) faceva crashare gcc
    template<typename T> struct value_getter {
        T operator ()() {
            return T{};
        }
    };

    template<std::integral T, T Value> struct int_getter {
        T operator ()() {
            return Value;
        }
    };

    template<typename T, typename DefaultGetter = value_getter<T>>
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
    
    template<typename T> using vararg_converter = variable_converter<convert_lvalue<T>>;
    template<typename T> using varargs_base = std::ranges::transform_view<arg_list, vararg_converter<T>>;
    template<typename T, size_t Minargs = 0> struct varargs : varargs_base<T> {
        using var_type = T;
        template<typename U>
        varargs(U &&obj) : varargs_base<T>(std::forward<U>(obj), vararg_converter<T>{}) {}
    };

    template<typename T> struct is_variable : std::bool_constant<! std::is_void_v<convert_rvalue<T>>> {};

    template<typename T> struct is_optional : std::false_type {};
    template<typename T, typename Getter> struct is_optional<optional<T, Getter>> : std::bool_constant<is_variable<T>{}> {};

    template<typename T> struct is_varargs : std::false_type {};
    template<typename T, size_t Minargs> struct is_varargs<varargs<T, Minargs>> : std::bool_constant<is_variable<T>{}> {};

    namespace detail {
        template<bool Req, typename ... Ts> struct check_args {};
        template<bool Req> struct check_args<Req> {
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
        class check_args<Req, First, Ts ...> {
        private:
            static constexpr bool var = is_variable<std::decay_t<First>>{};
            static constexpr bool opt = is_optional<std::decay_t<First>>{};
            static constexpr bool vec = is_varargs<std::decay_t<First>>{};

            // Req indica se il parametro precedente non era opt
            using recursive = check_args<Req ? var : !opt, Ts ...>;

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
    }

    template<typename Function> struct check_args {};
    template<typename T, typename ... Ts> struct check_args<T(*)(Ts ...)> : detail::check_args<true, Ts ...> {
        static constexpr bool has_context = false;
        using types = util::type_list<Ts ...>;
        using return_type = T;
    };

    template<typename T, typename Reader, typename ... Ts> requires std::is_same_v<std::remove_cv_t<Reader>, class reader>
    struct check_args<T(*)(Reader*, Ts ...)> : detail::check_args<true, Ts ...> {
        static constexpr bool has_context = true;
        using types = util::type_list<Ts ...>;
        using return_type = T;
    };

    template<typename T> using function_types_t = typename check_args<T>::types;
    template<typename T> constexpr bool has_context_v = check_args<T>::has_context;

    template<typename TypeList, size_t I> inline decltype(auto) get_arg(arg_list &args) {
        using type = std::decay_t<util::get_nth_t<I, TypeList>>;
        if constexpr (is_optional<std::decay_t<type>>{}) {
            using opt_type = typename type::value_type;
            if (I >= args.size()) {
                return type();
            } else {
                return type(variable_converter<convert_rvalue<opt_type>>{}(args[I]));
            }
        } else if constexpr (is_varargs<std::decay_t<type>>{}) {
            return type(arg_list(args.begin() + I, args.end()));
        } else {
            return variable_converter<convert_rvalue<type>>{}(args[I]);
        }
    }

    template<typename Function, typename ReturnType>
    concept valid_function_returning = std::default_initializable<Function> && requires (Function fun) {
        +fun;
        requires check_args<decltype(+fun)>::valid;
        requires std::convertible_to<typename check_args<decltype(+fun)>::return_type, ReturnType>;
    };

    template<typename ReturnType>
    class function_handler_base {
    private:
        ReturnType (*const m_fun) (class reader *ctx, arg_list);

        // l'operatore unario + converte una funzione lambda senza capture
        // in puntatore a funzione. In questo modo il compilatore può
        // dedurre i tipi dei parametri della funzione tramite i template

        // function_handler_base rappresenta una closure che passa automaticamente gli argomenti
        // da arg_list alla funzione fun, convertendoli nei tipi giusti
        template<typename Function> static ReturnType call_function(class reader *ctx, arg_list args) {
            using types = function_types_t<decltype(+Function{})>;
            return [] <size_t ... Is> (class reader *ctx, arg_list &args, std::index_sequence<Is...>) {
                if constexpr (has_context_v<decltype(+Function{})>) {
                    return Function{}(ctx, get_arg<types, Is>(args) ...);
                } else {
                    return Function{}(get_arg<types, Is>(args) ...);
                }
            }(ctx, args, std::make_index_sequence<types::size>{});
        }

    public:
        const size_t minargs;
        const size_t maxargs;

        template<valid_function_returning<ReturnType> Function>
        function_handler_base(Function fun)
            : m_fun(call_function<Function>)
            , minargs(check_args<decltype(+fun)>::minargs)
            , maxargs(check_args<decltype(+fun)>::maxargs) {}

        ReturnType operator ()(class reader *ctx, simple_stack<variable> &stack, size_t numargs) const {
            if constexpr (std::is_void_v<ReturnType>) {
                m_fun(ctx, arg_list(stack.end() - numargs, stack.end()));
                stack.resize(stack.size() - numargs);
            } else {
                auto ret = m_fun(ctx, arg_list(stack.end() - numargs, stack.end()));
                stack.resize(stack.size() - numargs);
                return ret;
            }
        }
    };

    using function_handler = function_handler_base<variable>;
    using sys_function_handler = function_handler_base<void>;

    using function_map = std::map<std::string, function_handler, std::less<>>;
    using function_iterator = function_map::const_iterator;

    using sys_function_map = std::map<std::string, sys_function_handler, std::less<>>;
    using sys_function_iterator = sys_function_map::const_iterator;

    extern const function_map function_lookup;
    extern const sys_function_map sys_function_lookup;

}

#endif