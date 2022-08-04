#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <ranges>
#include <span>
#include <map>

#include "variable.h"

namespace bls {

    template<typename T> struct variable_converter {};

    template<> struct variable_converter<variable> {
        const variable &operator()(const variable &var) const { return var.deref(); }
    };
    template<> struct variable_converter<std::string> {
        const std::string &operator()(const variable &var) const { return var.as_string(); }
    };
    template<> struct variable_converter<fixed_point> {
        fixed_point operator()(const variable &var) const { return var.as_number(); }
    };
    template<> struct variable_converter<datetime> {
        datetime operator()(const variable &var) const { return var.as_date(); }
    };
    template<> struct variable_converter<bool> {
        bool operator()(const variable &var) const { return var.is_true(); }
    };
    template<typename T> requires std::derived_from<string_state, T> struct variable_converter<T> {
        T operator()(const variable &var) const { return var.as_view(); }
    };
    template<std::integral T> struct variable_converter<T> {
        T operator()(const variable &var) const { return var.as_int(); }
    };
    template<std::floating_point T> struct variable_converter<T> {
        T operator()(const variable &var) const { return var.as_double(); }
    };

    template<typename T> using vector_view = std::ranges::transform_view<std::span<const variable>, variable_converter<T>>;

    template<typename T> struct variable_converter<vector_view<T>> {
        vector_view<T> operator()(const variable &var) const {
            if (!var.is_array()) return {};
            return std::ranges::transform_view(std::span{var.as_array()}, variable_converter<T>{});
        }
    };

    template<typename T> struct default_constructed { static inline const T value{}; };

    template<typename T, typename DefValue = default_constructed<T>>
    struct optional : T {};

    template<std::integral T, typename DefValue>
    struct optional<T, DefValue> {
        T m_value;
        operator T() const { return m_value; }
    };

    template<int Value>     using optional_int =    optional<int,    std::integral_constant<int, Value>>;
    template<size_t Value>  using optional_size =   optional<size_t, std::integral_constant<size_t, Value>>;

    using arg_list = std::ranges::subrange<typename util::simple_stack<variable>::const_iterator>;
    
    template<typename T> using varargs_base = std::ranges::transform_view<arg_list, variable_converter<T>>;
    template<typename T, size_t Minargs = 0> struct varargs : varargs_base<T> {
        using var_type = T;
        template<std::ranges::input_range U> requires std::ranges::view<U>
        varargs(U &&obj) : varargs_base<T>(std::forward<U>(obj), variable_converter<T>{}) {}
    };

    template<typename T> concept argument_type = requires(const variable &var) {
        variable_converter<T>{}(var);
    };

    class reader;

    constexpr size_t args_infinite = std::numeric_limits<size_t>::max();

    template<typename TList> struct count_args {};
    template<typename TList> struct count_args_optional {};

    template<> struct count_args<util::type_list<>> {
        static constexpr size_t minargs = 0;
        static constexpr size_t maxargs = 0;
    };

    template<argument_type First, typename ... Ts>
    struct count_args<util::type_list<First, Ts ...>> {
        using recursive = count_args<util::type_list<Ts ...>>;
        static constexpr size_t minargs = 1 + recursive::minargs;
        static constexpr size_t maxargs = recursive::maxargs == args_infinite ? args_infinite : 1 + recursive::maxargs;
    };

    template<argument_type T, size_t Minargs>
    struct count_args<util::type_list<varargs<T, Minargs>>> {
        static constexpr size_t minargs = Minargs;
        static constexpr size_t maxargs = args_infinite;
    };

    template<> struct count_args_optional<util::type_list<>> {
        static constexpr size_t maxargs = 0;
    };

    template<argument_type T, typename DefValue, typename ... Ts>
    struct count_args<util::type_list<optional<T, DefValue>, Ts ...>> : count_args_optional<util::type_list<optional<T, DefValue>, Ts ...>> {
        static constexpr size_t minargs = 0;
    };

    template<argument_type T, typename DefValue, typename ... Ts>
    struct count_args_optional<util::type_list<optional<T, DefValue>, Ts ...>> {
        using recursive = count_args_optional<util::type_list<Ts ...>>;
        static constexpr size_t maxargs = recursive::maxargs == args_infinite ? args_infinite : 1 + recursive::maxargs;
    };

    template<argument_type T>
    struct count_args_optional<util::type_list<varargs<T>>> {
        static constexpr size_t maxargs = args_infinite;
    };

    template<typename Function> struct function_unwrapper{};

    template<typename ReturnType, typename Reader, typename ... Ts> requires std::is_same_v<std::remove_cv_t<Reader>, reader>
    struct function_unwrapper<ReturnType(*)(Reader*, Ts ...)> {
        using return_type = ReturnType;
        using arg_types = util::type_list<std::decay_t<Ts> ...>;
        static constexpr bool has_context = true;
    };

    template<typename ReturnType, typename ... Ts>
    struct function_unwrapper<ReturnType(*)(Ts ...)> {
        using return_type = ReturnType;
        using arg_types = util::type_list<std::decay_t<Ts> ...>;
        static constexpr bool has_context = false;
    };

    // l'operatore unario + converte una funzione lambda senza capture
    // in puntatore a funzione. In questo modo il compilatore può
    // dedurre i tipi dei parametri della funzione tramite i template
    template<typename T> concept function_object = std::default_initializable<T> && requires {
        &T::operator();
    };

    template<function_object Function> struct function_unwrapper<Function> : function_unwrapper<decltype(+Function{})> {};

    template<typename Function> using function_return_type_t = typename function_unwrapper<Function>::return_type;
    template<typename Function> using function_arg_types_t = typename function_unwrapper<Function>::arg_types;
    template<typename Function> constexpr bool function_has_context_v = function_unwrapper<Function>::has_context;
    template<typename Function> constexpr size_t function_minargs_v = count_args<function_arg_types_t<Function>>::minargs;
    template<typename Function> constexpr size_t function_maxargs_v = count_args<function_arg_types_t<Function>>::maxargs;

    template<typename Function>
    concept valid_args = requires {
        function_minargs_v<Function>;
        function_maxargs_v<Function>;
    };

    template<typename T>
    concept valid_return_type = std::is_void_v<T> || std::convertible_to<T, variable>;

    template<typename T>
    concept valid_function = function_object<T> && valid_args<T> && valid_return_type<function_return_type_t<T>>;

    template<typename T> struct argument_getter{};
    template<argument_type T> struct argument_getter<T> {
        decltype(auto) operator()(arg_list &args, size_t index) const {
            return variable_converter<T>{}(args[index]);
        }
    };
    template<argument_type T, typename DefValue> struct argument_getter<optional<T, DefValue>> {
        optional<T, DefValue> operator()(arg_list &args, size_t index) const {
            if (index >= args.size()) {
                return {DefValue::value};
            } else {
                return {variable_converter<T>{}(args[index])};
            }
        }
    };
    template<argument_type T, size_t Minargs> struct argument_getter<varargs<T, Minargs>> {
        varargs<T, Minargs> operator()(arg_list &args, size_t index) const {
            return arg_list(args.begin() + index, args.end());
        }
    };

    template<typename TList, size_t I> static decltype(auto) get_arg(arg_list &args) {
        return argument_getter<util::get_nth_t<I, TList>>{}(args, I);
    }

    // rappresenta una closure che passa automaticamente gli argomenti
    // dallo stack ad una funzione lambda, convertendoli nei tipi giusti
    class function_handler {
    private:
        variable (*const m_fun) (class reader *ctx, arg_list);

        template<typename Function> static variable call_function(class reader *ctx, arg_list args) {
            using types = function_arg_types_t<Function>;
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

        template<typename Function>
        function_handler(Function)
            : m_fun(call_function<Function>)
            , minargs(function_minargs_v<Function>)
            , maxargs(function_maxargs_v<Function>)
            , returns_value(!std::is_void_v<function_return_type_t<Function>>)
        { static_assert(valid_function<Function>); }

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