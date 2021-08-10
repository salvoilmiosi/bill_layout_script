#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <boost/preprocessor.hpp>
#include <algorithm>
#include <string>
#include <map>

#include "magic_enum.hpp"

namespace enums {
    namespace detail {
        template<size_t S, typename ... Ts> struct sized_int {};
        template<size_t S, typename ... Ts> using sized_int_t = typename sized_int<S, Ts ...>::type;

        template<size_t S, typename T> constexpr bool fits_in_v = S <= std::numeric_limits<T>::max();

        template<size_t S, typename T> struct sized_int<S, T>
            : std::enable_if<fits_in_v<S, T>, T> {};
        template<size_t S, typename First, typename ... Ts> struct sized_int<S, First, Ts...>
            : std::conditional<fits_in_v<S, First>, First, sized_int_t<S, Ts...>> {};
    }

    template<size_t S> using sized_int_t = detail::sized_int_t<S, uint8_t, uint16_t, uint32_t, uint64_t>;

    template<typename T> concept is_enum = magic_enum::is_scoped_enum_v<T>;

    template<is_enum T> struct data_type{};
    template<is_enum T> using data_type_t = typename data_type<T>::type;

    template<is_enum T, typename = void> struct is_data_enum : std::false_type {};
    template<is_enum T> struct is_data_enum<T, std::void_t<data_type_t<T>>> : std::true_type {};

    template<typename T> concept data_enum = is_data_enum<T>::value;

    template<data_enum T> const data_type_t<T> &get_data(T value) = delete;

    template<is_enum T, T Enum> struct get_type{};
    template<is_enum T, T Enum> using get_type_t = typename get_type<T, Enum>::type;

    template<is_enum T> struct is_type_enum : std::false_type {};

    template<typename T> concept type_enum = is_type_enum<T>::value;

    template<is_enum T> static constexpr bool is_flags_enum() {
        size_t i = 1;
        for (auto value : magic_enum::enum_values<T>()) {
            if (value != static_cast<T>(i)) return false;
            i <<= 1;
        }
        return true;
    }
    template<typename T> concept flags_enum = is_flags_enum<T>();

    template<is_enum T> constexpr size_t size() {
        return magic_enum::enum_count<T>();
    }

    template<is_enum T> constexpr T invalid_enum_value = static_cast<T>(std::numeric_limits<std::underlying_type_t<T>>::max());

    template<is_enum T> constexpr T from_string(std::string_view str) {
        if (auto value = magic_enum::enum_cast<T>(str)) {
            return value.value();
        } else {
            return invalid_enum_value<T>;
        }
    }

    template<is_enum T> constexpr std::string_view to_string(T value) {
        return magic_enum::enum_name(value);
    }

    template<data_enum T>
    T find_enum_index(std::string_view name) {
        auto is_name = [&](std::string_view str) {
            return name == str;
        };
        const auto &values = magic_enum::enum_values<T>();
        if (auto it = std::ranges::find_if(values,
            [&](const auto &data) {
                if constexpr (std::equality_comparable_with<std::string_view, data_type_t<T>>) {
                    return is_name(data);
                } else {
                    return std::ranges::any_of(data, is_name);
                }
            }, get_data<T>); it != values.end()) return *it;
        return invalid_enum_value<T>;
    }

    template<flags_enum T>
    class bitset {
    private:
        using flags_t = std::underlying_type_t<T>;
        flags_t m_value{0};

    public:
        constexpr bitset() = default;
        constexpr bitset(flags_t value) : m_value(value) {}

        constexpr bool empty() const {
            return m_value == 0;
        }

        constexpr bool check(T value) const {
            return m_value & static_cast<flags_t>(value);
        }

        constexpr void set(T value) {
            m_value |= static_cast<flags_t>(value);
        }

        constexpr void unset(T value) {
            m_value &= ~static_cast<flags_t>(value);
        }

        constexpr void toggle(T value) {
            m_value ^= static_cast<flags_t>(value);
        }

        constexpr void reset() {
            m_value = 0;
        }

        constexpr flags_t data() const {
            return m_value;
        }

        friend std::ostream &operator << (std::ostream &out, const bitset &flags) {
            for (auto value : magic_enum::enum_values<T>()) {
                if (flags.check(value)) {
                    out << value << ' ';
                }
            }
            return out;
        }
    };
}

template<enums::is_enum T>
std::ostream &operator << (std::ostream &out, const T &value) {
    return out << enums::to_string(value);
}

#define DO_NOTHING(...)

#define HELPER1(...) ((__VA_ARGS__)) HELPER2
#define HELPER2(...) ((__VA_ARGS__)) HELPER1
#define HELPER1_END
#define HELPER2_END
#define ADD_PARENTHESES(sequence) BOOST_PP_CAT(HELPER1 sequence,_END)

#define STRIP_PARENS_ARGS(...) __VA_ARGS__
#define STRIP_PARENS(x) x

#define ENUM_ELEMENT_NAME(elementTuple) BOOST_PP_TUPLE_ELEM(0, elementTuple)

#define CREATE_ENUM_ELEMENT(r, enumName, i, elementTuple) (ENUM_ELEMENT_NAME(elementTuple) = i)
#define CREATE_FLAG_ELEMENT(r, enumName, i, elementTuple) (ENUM_ELEMENT_NAME(elementTuple) = 1 << i)

#define CREATE_ENUM_ELEMENT_MAX_VALUE(n) n - 1
#define CREATE_FLAG_ELEMENT_MAX_VALUE(n) 1 << (n - 1)

#define ENUM_INT(enum_value_fun, elementTupleSeq) enums::sized_int_t<enum_value_fun##_MAX_VALUE(BOOST_PP_SEQ_SIZE(elementTupleSeq))>

#define UNROLL_ELEMENT_VALUE(elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 1), DO_NOTHING, STRIP_PARENS) \
    (STRIP_PARENS_ARGS BOOST_PP_TUPLE_POP_FRONT(elementTuple))

#define GENERATE_CASE_GET_DATA(r, enumName, elementTuple) { enumName::ENUM_ELEMENT_NAME(elementTuple), { UNROLL_ELEMENT_VALUE(elementTuple) }},

#define CREATE_ENUM_GET_DATA(enumName, elementTupleSeq, valueType) \
template<> struct data_type<enumName> { using type = valueType; }; \
template<> inline const data_type_t<enumName> &get_data<enumName>(enumName value) { \
    static const std::map<enumName, valueType> data { \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_DATA, enumName, elementTupleSeq) \
    }; \
    return data.at(value); \
}

#define GENERATE_GET_TYPE_BASE(enumName) \
template<> struct is_type_enum<enumName> : std::true_type {}; \
template<enumName Enum> struct get_type<enumName, Enum> { using type = void; }; \

#define GENERATE_GET_TYPE_STRUCT(enumName, elementTuple) \
    template<> struct get_type<enumName, enumName::ENUM_ELEMENT_NAME(elementTuple)> { using type = BOOST_PP_TUPLE_ELEM(1, elementTuple); };

#define GENERATE_CASE_GET_TYPE(r, enumName, elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 1), \
        DO_NOTHING, GENERATE_GET_TYPE_STRUCT)(enumName, elementTuple)

#define CREATE_ENUM_GET_TYPE(enumName, elementTupleSeq, ...) \
    GENERATE_GET_TYPE_BASE(enumName) \
    BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_TYPE, enumName, elementTupleSeq)

#define IMPL_DEFINE_ENUM(enumName, elementTupleSeq, enum_value_fun, enum_data_fun, ...) \
enum class enumName : ENUM_INT(enum_value_fun, elementTupleSeq) { \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH_I(enum_value_fun, enumName, elementTupleSeq)) \
}; namespace enums { \
enum_data_fun(enumName, elementTupleSeq, __VA_ARGS__) }

#define IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, elementTupleSeq, enum_value_fun, enum_data_fun, ...) \
    enum class enumName : ENUM_INT(enum_value_fun, elementTupleSeq) { \
        BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH_I(enum_value_fun, enumName, elementTupleSeq)) \
    }; \
} namespace enums { using namespace namespaceName; \
enum_data_fun(enumName, elementTupleSeq, __VA_ARGS__) \
} namespace namespaceName {

#define DEFINE_ENUM(enumName, enumElements) \
    IMPL_DEFINE_ENUM(enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, DO_NOTHING)
#define DEFINE_ENUM_FLAGS(enumName, enumElements) \
    IMPL_DEFINE_ENUM(enumName, ADD_PARENTHESES(enumElements), CREATE_FLAG_ELEMENT, DO_NOTHING)

#define DEFINE_ENUM_DATA(enumName, valueType, enumElements) \
    IMPL_DEFINE_ENUM(enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, CREATE_ENUM_GET_DATA, valueType)
#define DEFINE_ENUM_FLAGS_DATA(enumName, valueType, enumElements) \
    IMPL_DEFINE_ENUM(enumName, ADD_PARENTHESES(enumElements), CREATE_FLAG_ELEMENT, CREATE_ENUM_GET_DATA, valueType)

#define DEFINE_ENUM_TYPES(enumName, enumElements) \
    IMPL_DEFINE_ENUM(enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, CREATE_ENUM_GET_TYPE)

#define DEFINE_ENUM_IN_NS(namespaceName, enumName, enumElements) \
    IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, DO_NOTHING)
#define DEFINE_ENUM_FLAGS_IN_NS(namespaceName, enumName, enumElements) \
    IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, ADD_PARENTHESES(enumElements), CREATE_FLAG_ELEMENT, DO_NOTHING)

#define DEFINE_ENUM_DATA_IN_NS(namespaceName, enumName, valueType, enumElements) \
    IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, CREATE_ENUM_GET_DATA, valueType)
#define DEFINE_ENUM_FLAGS_DATA_IN_NS(namespaceName, enumName, valueType, enumElements) \
    IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, ADD_PARENTHESES(enumElements), CREATE_FLAG_ELEMENT, CREATE_ENUM_GET_DATA, valueType)

#define DEFINE_ENUM_TYPES_IN_NS(namespaceName, enumName, enumElements) \
    IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, ADD_PARENTHESES(enumElements), CREATE_ENUM_ELEMENT, CREATE_ENUM_GET_TYPE)

#endif