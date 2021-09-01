#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <ranges>
#include <span>
#include <map>

#include "variable.h"
#include "utils.h"
#include "type_list.h"

namespace bls {

    template<typename T> struct variable_converter {};

    #define DEFINE_CONVERTER(t, cmd) template<> struct variable_converter<t> { t operator()(const variable &var) const { return cmd; } };

    DEFINE_CONVERTER(std::string_view,  var.as_view())
    DEFINE_CONVERTER(string_state,      var.as_view())
    DEFINE_CONVERTER(fixed_point,       var.as_number())
    DEFINE_CONVERTER(datetime,          var.as_date())
    DEFINE_CONVERTER(size_t,            var.as_int())
    DEFINE_CONVERTER(int,               var.as_int())
    DEFINE_CONVERTER(float,             var.as_double())
    DEFINE_CONVERTER(double,            var.as_double())
    DEFINE_CONVERTER(bool,              var.is_true())

    #undef DEFINE_CONVERTER

    template<> struct variable_converter<variable> {
        const variable &operator()(const variable &var) const {
            return var.deref();
        }
    };

    template<> struct variable_converter<std::string> {
        const std::string &operator()(const variable &var) const {
            return var.as_string();
        }
    };

    template<typename T> using vector_view = std::ranges::transform_view<std::span<const variable>, variable_converter<T>>;

    template<typename T> struct variable_converter<vector_view<T>> {
        vector_view<T> operator()(const variable &var) const {
            if (!var.is_array()) return {};
            return std::ranges::transform_view(std::span{var.as_array()}, variable_converter<T>{});
        }
    };

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

    using arg_list = std::ranges::subrange<typename util::simple_stack<variable>::const_iterator>;
    
    template<typename T> using varargs_base = std::ranges::transform_view<arg_list, variable_converter<T>>;
    template<typename T, size_t Minargs = 0> struct varargs : varargs_base<T> {
        using var_type = T;
        template<typename U>
        varargs(U &&obj) : varargs_base<T>(std::forward<U>(obj), variable_converter<T>{}) {}
    };

    template<typename T, typename = void> struct is_variable : std::false_type {};
    template<typename T> struct is_variable<T, std::void_t<decltype(variable_converter<T>{}(std::declval<variable>()))>> : std::true_type {};

    template<typename T> struct is_optional : std::false_type {};
    template<typename T, typename Getter> struct is_optional<optional<T, Getter>> : is_variable<T> {};

    template<typename T> struct is_varargs : std::false_type {};
    template<typename T, size_t Minargs> struct is_varargs<varargs<T, Minargs>> : is_variable<T> {};

    class reader;

    namespace detail {
        template<typename T> struct vararg_minargs {
            static constexpr size_t value = 0;
        };

        template<typename T, size_t Minargs> struct vararg_minargs<varargs<T, Minargs>> {
            static constexpr size_t value = Minargs;
        };

        template<typename T> constexpr size_t vararg_minargs_v = vararg_minargs<T>::value;

        template<bool Req, typename ReturnType, typename ... Ts> struct check_args_helper {};
        template<bool Req, typename ReturnType> struct check_args_helper<Req, ReturnType> {
            static constexpr bool minargs = 0;
            static constexpr bool maxargs = 0;

            static constexpr bool valid = true;

            using return_type = ReturnType;
            using types = util::type_list<>;
        };

        template<bool Req, typename ReturnType, typename First, typename ... Ts>
        class check_args_helper<Req, ReturnType, First, Ts ...> {
        private:
            static constexpr bool var = is_variable<std::decay_t<First>>{};
            static constexpr bool opt = is_optional<std::decay_t<First>>{};
            static constexpr bool vec = is_varargs<std::decay_t<First>>{};

            // Req indica se il parametro precedente non era opt
            using recursive = check_args_helper<Req ? var : !opt, ReturnType, Ts ...>;

        public:
            // Conta il numero di tipi che non sono opzionali
            static constexpr size_t minargs = Req && var ? 1 + recursive::minargs : vec ? vararg_minargs_v<First> : recursive::minargs;
            // Conta il numero totale di tipi o INT_MAX se l'ultimo è varargs<T>
            static constexpr size_t maxargs = (vec || recursive::maxargs == std::numeric_limits<size_t>::max()) ?
                std::numeric_limits<size_t>::max() : 1 + recursive::maxargs;

            // true solo se i tipi degli argomenti seguono il pattern (var*N, opt*M, [vec])
            // Tutti gli argomenti opzionali devono essere dopo quelli richiesti
            static constexpr bool valid = vec ? sizeof...(Ts) == 0 && (Req || vararg_minargs_v<First> == 0) : (Req || opt) && recursive::valid;

            using return_type = ReturnType;
            using types = util::type_list<First, Ts ...>;
        };

        template<typename Function> struct check_args {};

        template<typename ReturnType, typename Reader, typename ... Ts> requires std::is_same_v<std::remove_cv_t<Reader>, reader>
        struct check_args<ReturnType(*)(Reader*, Ts ...)> : check_args_helper<true, ReturnType, Ts ...> {
            static constexpr bool has_context = true;
        };

        template<typename ReturnType, typename ... Ts>
        struct check_args<ReturnType(*)(Ts ...)> : check_args_helper<true, ReturnType, Ts ...> {
            static constexpr bool has_context = false;
        };
    }

    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template
    template<typename Function> using check_args = detail::check_args<decltype(+Function{})>;

    template<typename Function> using function_return_type_t = typename check_args<Function>::return_type;
    template<typename Function> using function_types_t = typename check_args<Function>::types;
    template<typename Function> constexpr size_t function_minargs_v = check_args<Function>::minargs;
    template<typename Function> constexpr size_t function_maxargs_v = check_args<Function>::maxargs;
    template<typename Function> constexpr bool function_has_context_v = check_args<Function>::has_context;

    template<typename T>
    concept valid_return_type = std::is_void_v<T> || std::convertible_to<T, variable>;

    template<typename T>
    concept valid_function = std::default_initializable<T>
        && check_args<T>::valid
        && valid_return_type<function_return_type_t<T>>;

    // rappresenta una closure che passa automaticamente gli argomenti
    // dallo stack ad una funzione lambda, convertendoli nei tipi giusti
    class function_handler {
    private:
        variable (*const m_fun) (class reader *ctx, arg_list);

        template<typename TypeList, size_t I> static decltype(auto) get_arg(arg_list &args) {
            using type = std::decay_t<util::get_nth_t<I, TypeList>>;
            if constexpr (is_optional<std::decay_t<type>>{}) {
                using opt_type = typename type::value_type;
                if (I >= args.size()) {
                    return type();
                } else {
                    return type(variable_converter<opt_type>{}(args[I]));
                }
            } else if constexpr (is_varargs<std::decay_t<type>>{}) {
                return type(arg_list(args.begin() + I, args.end()));
            } else {
                return variable_converter<type>{}(args[I]);
            }
        }

        template<typename Function> static variable call_function(class reader *ctx, arg_list args) {
            using types = function_types_t<Function>;
            constexpr auto fun = [] <size_t ... Is> (class reader *ctx, arg_list &args, std::index_sequence<Is...>) {
                if constexpr (function_has_context_v<Function>) {
                    return Function{}(ctx, get_arg<types, Is>(args) ...);
                } else {
                    return Function{}(get_arg<types, Is>(args) ...);
                }
            };
            if constexpr (!std::is_void_v<function_return_type_t<Function>>) {
                return fun(ctx, args, std::make_index_sequence<types::size>{});
            } else {
                fun(ctx, args, std::make_index_sequence<types::size>{});
                return {};
            }
        }

    public:
        const size_t minargs;
        const size_t maxargs;
        const bool returns_value;

        template<valid_function Function>
        function_handler(Function)
            : m_fun(call_function<Function>)
            , minargs(function_minargs_v<Function>)
            , maxargs(function_maxargs_v<Function>)
            , returns_value(!std::is_void_v<function_return_type_t<Function>>) {}

        variable operator()(class reader *ctx, arg_list args) const {
            return m_fun(ctx, args);
        }
    };

    using function_map = util::string_map<function_handler>;
    using function_iterator = function_map::const_iterator;

    class function_lookup {
    private:
        static const function_map functions;

    public:
        static function_iterator find(std::string_view name) { return functions.find(name); }
        static bool valid(function_iterator it) { return it != functions.end(); }
    };

}

#endif