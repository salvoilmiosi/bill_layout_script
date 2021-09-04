#ifndef __ENUM_SEQUENCE_H__
#define __ENUM_SEQUENCE_H__

#include "enums.h"
#include "type_list.h"

namespace enums {
    template<reflected_enum auto Value> struct enum_constant {};

    template<reflected_enum auto ... Values> using enum_sequence = util::type_list<enum_constant<Values>...>;
    
    namespace detail {
        template<reflected_enum T, typename ISeq> struct make_enum_sequence{};
        template<reflected_enum T, size_t ... Is> struct make_enum_sequence<T, std::index_sequence<Is...>> {
            using type = enum_sequence<enum_values_v<T>[Is]...>;
        };

        template<template<reflected_enum auto> typename Filter>
        struct enum_filter_wrapper {
            template<typename EnumConst> struct type{};
            template<reflected_enum auto Value> struct type<enum_constant<Value>> : Filter<Value> {};
        };
    }

    template<reflected_enum T> using make_enum_sequence = typename detail::make_enum_sequence<T, std::make_index_sequence<size_v<T>>>::type;

    template<template<reflected_enum auto> typename Filter, typename ESeq>
    using filter_enum_sequence = util::type_list_filter_t<
        detail::enum_filter_wrapper<Filter>::template type, ESeq>;

    namespace detail {
        template<reflected_enum auto Enum> struct enum_type_or_monostate { using type = std::monostate; };
        template<reflected_enum auto Enum> requires has_type<Enum> struct enum_type_or_monostate<Enum> { using type = enum_type_t<Enum>; };

        template<typename EnumSeq> struct enum_variant{};
        template<reflected_enum Enum, Enum ... Es> struct enum_variant<enum_sequence<Es...>> {
            using type = std::variant<typename enum_type_or_monostate<Es>::type ... >;
        };
    }

    template<reflected_enum Enum> using enum_variant = typename detail::enum_variant<make_enum_sequence<Enum>>::type;

    template<reflected_enum Enum> auto get_data(Enum value) {
        constexpr auto data_array = []<Enum ... Es>(enum_sequence<Es...>) {
            return std::array{ enum_data_v<Es> ... };
        }(make_enum_sequence<Enum>());
        return data_array[indexof(value)];
    }

}

#endif