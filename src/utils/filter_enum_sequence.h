#ifndef __FILTER_ENUM_SEQUENCE_H__
#define __FILTER_ENUM_SEQUENCE_H__

#include "enums.h"
#include "type_list.h"

namespace enums {

    namespace detail {
        template<template<reflected_enum auto> typename Filter>
        struct enum_filter_wrapper {
            template<typename EnumConst> struct type{};
            template<reflected_enum auto Value> struct type<enum_tag_t<Value>> : Filter<Value> {};
        };

        template<typename ESeq> struct enum_sequence_to_type_list{};
        template<typename ESeq> using enum_sequence_to_type_list_t = typename enum_sequence_to_type_list<ESeq>::type;
        template<reflected_enum auto ... Es> struct enum_sequence_to_type_list<enum_sequence<Es...>> {
            using type = util::type_list<enum_tag_t<Es>...>;
        };

        template<typename TList> struct type_list_to_enum_sequence{};
        template<typename TList> using type_list_to_enum_sequence_t = typename type_list_to_enum_sequence<TList>::type;
        template<reflected_enum auto ... Es> struct type_list_to_enum_sequence<util::type_list<enum_tag_t<Es>...>> {
            using type = enum_sequence<Es ...>;
        };
    }

    template<template<reflected_enum auto> typename Filter, typename ESeq>
    using filter_enum_sequence = detail::type_list_to_enum_sequence_t<util::type_list_filter_t<
        detail::enum_filter_wrapper<Filter>::template type, detail::enum_sequence_to_type_list_t<ESeq>>>;

}

#endif