#ifndef __TYPE_LIST_H__
#define __TYPE_LIST_H__

#include <type_traits>
#include "enums.h"

namespace util {

    template<typename ... Ts> struct type_list {
        static constexpr size_t size = sizeof...(Ts);
    };

    template<size_t N, typename TList> struct get_nth {};
    template<size_t I, typename TList> using get_nth_t = typename get_nth<I, TList>::type;

    template<size_t N, typename First, typename ... Ts> struct get_nth<N, type_list<First, Ts...>> {
        using type = typename get_nth<N-1, type_list<Ts ...>>::type;
    };

    template<typename First, typename ... Ts> struct get_nth<0, type_list<First, Ts ...>> {
        using type = First;
    };

    template<typename T, typename TList> struct type_list_contains{};
    template<typename T, typename TList> static constexpr bool type_list_contains_v = type_list_contains<T, TList>::value;

    template<typename T> struct type_list_contains<T, type_list<>> : std::false_type {};

    template<typename T, typename ... Ts> struct type_list_contains<T, type_list<T, Ts...>> : std::true_type {};
    template<typename T, typename First, typename ... Ts>
    struct type_list_contains<T, type_list<First, Ts...>> : type_list_contains<T, type_list<Ts...>> {};

    template<typename T, typename TList> struct type_list_indexof{};
    template<typename T, typename TList> static constexpr size_t type_list_indexof_v = type_list_indexof<T, TList>::value;

    template<typename T, typename First, typename ... Ts> struct type_list_indexof<T, type_list<First, Ts...>> {
        static constexpr size_t value = 1 + type_list_indexof_v<T, type_list<Ts ...>>;
    };
    template<typename T, typename ... Ts> struct type_list_indexof<T, type_list<T, Ts...>> {
        static constexpr size_t value = 0;
    };

    template<typename T, typename TList> struct type_list_append{};
    template<typename T, typename TList> using type_list_append_t = typename type_list_append<T, TList>::type;

    template<typename T, typename ... Ts> struct type_list_append<T, type_list<Ts...>> {
        using type = type_list<Ts..., T>;
    };

    template<bool Cond, typename T, typename TList> using type_list_append_if_t =
        std::conditional_t<Cond, type_list_append_t<T, TList>, TList>;

    template<typename T> struct variant_type_list{};
    template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

    template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
        using type = util::type_list<Ts...>;
    };

    namespace detail {
        template<typename T> using variant_type = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

        template<typename EnumSeq> struct enum_variant{};
        template<enums::type_enum Enum, Enum ... Es> struct enum_variant<enums::enum_sequence<Es...>> {
            using type = std::variant<variant_type<enums::get_type_t<Es>> ... >;
        };

        template<typename EnumSeq> struct enum_type_list{};
        template<enums::type_enum Enum, Enum ... Es> struct enum_type_list<enums::enum_sequence<Es...>> {
            using type = util::type_list<enums::get_type_t<Es> ... >;
        };
        
        template<template<typename ...> typename Filter, typename T, typename TList> struct filter_value{};

        template<template<typename, typename> typename Filter, typename T, typename TList>
        struct filter_value<Filter, T, TList> : Filter<T, TList> {};

        template<template<typename> typename Filter, typename T, typename TList>
        struct filter_value<Filter, T, TList> : Filter<T> {};

        template<template<typename ...> typename Filter, typename TList, typename TFrom> struct type_list_filter{};

        template<template<typename ...> typename Filter, typename TList>
        struct type_list_filter<Filter, TList, util::type_list<>> {
            using type = TList;
        };

        template<template<typename ...> typename Filter, typename TList, typename First, typename ... Ts>
        struct type_list_filter<Filter, TList, util::type_list<First, Ts...>> {
            using type = typename type_list_filter<
                Filter,
                util::type_list_append_if_t<
                    filter_value<Filter, First, TList>::value,
                    First,
                    TList
                >,
                util::type_list<Ts...>
            >::type;
        };
    }

    template<enums::type_enum Enum> using enum_type_list_t = typename detail::enum_type_list<enums::make_enum_sequence<Enum>>::type;
    template<enums::type_enum Enum> using enum_variant = typename detail::enum_variant<enums::make_enum_sequence<Enum>>::type;

    template<typename T, typename Variant> static constexpr size_t variant_indexof_v =
        type_list_indexof_v<T, variant_type_list_t<Variant>>;

    template<template<typename ...> typename Filter, typename TList>
    using type_list_filter_t = typename detail::type_list_filter<Filter, type_list<>, TList>::type;
}

#endif