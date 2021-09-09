#ifndef __ENUM_VARIANT_H__
#define __ENUM_VARIANT_H__

#include "enums.h"
#include <variant>

namespace enums {

    namespace detail {
        template<reflected_enum auto Enum> struct enum_type_or_monostate { using type = std::monostate; };
        template<reflected_enum auto Enum> requires has_type<Enum> struct enum_type_or_monostate<Enum> { using type = enum_type_t<Enum>; };

        template<typename EnumSeq> struct enum_variant{};
        template<reflected_enum Enum, Enum ... Es> struct enum_variant<enum_sequence<Es...>> {
            using type = std::variant<typename enum_type_or_monostate<Es>::type ... >;
        };
    }

    template<reflected_enum Enum> using enum_variant_base = typename detail::enum_variant<make_enum_sequence<Enum>>::type;

    template<reflected_enum Enum> struct enum_variant : enum_variant_base<Enum> {
        using base = enum_variant_base<Enum>;
        using base::base;

        template<Enum Value, typename ... Ts>
        enum_variant(enum_constant<Value>, Ts && ... args)
            : base(std::in_place_index<indexof(Value)>, std::forward<Ts>(args) ...) {}
        
        Enum enum_index() const {
            return enum_values_v<Enum>[base::index()];
        }

        template<Enum Value> const auto &get() const {
            return std::get<indexof(Value)>(*this);
        }

        template<Enum Value> auto &get() {
            return std::get<indexof(Value)>(*this);
        }
    };
    
    namespace detail {
        template<typename T> struct variant_type_list{};
        template<typename T> using variant_type_list_t = typename variant_type_list<T>::type;

        template<typename ... Ts> struct variant_type_list<std::variant<Ts...>> {
            using type = util::type_list<Ts...>;
        };
    }

    template<typename T, typename Variant> struct enum_variant_indexof{};
    template<typename T, reflected_enum Enum> struct enum_variant_indexof<T, enum_variant<Enum>> {
        static constexpr Enum value = enums::enum_values_v<Enum>[util::type_list_indexof_v<T, detail::variant_type_list_t<enum_variant_base<Enum>>>];
    };
    template<typename T, typename Variant> constexpr auto enum_variant_indexof_v = enum_variant_indexof<T, Variant>::value;

    template<typename Visitor, reflected_enum ... Es>
    auto visit(Visitor &&visitor, enum_variant<Es> const & ... vs) {
        return std::visit(std::forward<Visitor>(visitor), static_cast<enum_variant_base<Es> const &>(vs)...);
    }

    template<typename Visitor, reflected_enum ... Es>
    auto visit(Visitor &&visitor, enum_variant<Es> && ... vs) {
        return std::visit(std::forward<Visitor>(visitor), static_cast<enum_variant_base<Es> &&>(vs)...);
    }

    template<typename RetType, typename Visitor, reflected_enum ... Es>
    auto visit(Visitor &&visitor, enum_variant<Es> const & ... vs) {
        return std::visit<RetType>(std::forward<Visitor>(visitor), static_cast<enum_variant_base<Es> const &>(vs)...);
    }

    template<typename RetType, typename Visitor, reflected_enum ... Es>
    auto visit(Visitor &&visitor, enum_variant<Es> && ... vs) {
        return std::visit<RetType>(std::forward<Visitor>(visitor), static_cast<enum_variant_base<Es> &&>(vs)...);
    }
}

#endif