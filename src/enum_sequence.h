#ifndef __ENUM_SEQUENCE_H__
#define __ENUM_SEQUENCE_H__

#include "enums.h"
#include "type_list.h"

namespace enums {
    template<reflected_enum auto ... Values> struct enum_sequence{};
    
    namespace detail {
        template<reflected_enum T, typename ISeq> struct make_enum_sequence{};
        template<reflected_enum T, size_t ... Is> struct make_enum_sequence<T, std::index_sequence<Is...>> {
            using type = enum_sequence<enum_values_v<T>[Is]...>;
        };

        template<reflected_enum auto Value> struct enum_constant {
            static constexpr auto value = Value;
        };

        template<typename ESeq> struct enum_sequence_to_type_list{};
        template<typename ESeq> using enum_sequence_to_type_list_t = typename enum_sequence_to_type_list<ESeq>::type;
        template<reflected_enum auto ... Values>
        struct enum_sequence_to_type_list<enum_sequence<Values...>> {
            using type = util::type_list<enum_constant<Values>...>;
        };

        template<typename TList> struct enum_constant_type_list_to_sequence{};
        template<typename TList> using enum_constant_type_list_to_sequence_t = typename enum_constant_type_list_to_sequence<TList>::type;
        template<reflected_enum auto ... Values>
        struct enum_constant_type_list_to_sequence<util::type_list<enum_constant<Values>...>> {
            using type = enum_sequence<Values...>;
        };

        template<template<reflected_enum auto> typename Filter>
        struct enum_filter_wrapper {
            template<typename EnumConst> struct type{};
            template<reflected_enum auto Value> struct type<enum_constant<Value>> : Filter<Value> {};
        };
    }

    template<reflected_enum T> using make_enum_sequence = typename detail::make_enum_sequence<T, std::make_index_sequence<enum_values_v<T>.size()>>::type;

    template<template<reflected_enum auto> typename Filter, typename ESeq>
    using filter_enum_sequence = detail::enum_constant_type_list_to_sequence_t<
        util::type_list_filter_t<detail::enum_filter_wrapper<Filter>::template type,
            detail::enum_sequence_to_type_list_t<ESeq>>>;

    namespace detail {
        template<reflected_enum auto Enum> struct enum_type_or_monostate { using type = std::monostate; };
        template<reflected_enum auto Enum> requires has_type<Enum> struct enum_type_or_monostate<Enum> { using type = enum_type_t<Enum>; };

        template<typename EnumSeq> struct enum_variant{};
        template<reflected_enum Enum, Enum ... Es> struct enum_variant<enum_sequence<Es...>> {
            using type = std::variant<typename enum_type_or_monostate<Es>::type ... >;
        };
    }

    template<reflected_enum Enum> using enum_variant = typename detail::enum_variant<make_enum_sequence<Enum>>::type;

}

#endif