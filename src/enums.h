#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <boost/preprocessor.hpp>
#include <algorithm>
#include <string>

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

    template<typename T> struct enum_values {};
    template<typename T> constexpr auto enum_values_v = enum_values<T>::value;

    template<typename T> concept reflected_enum = std::is_enum_v<T> && requires {
        enum_values<T>::value;
    };

    template<reflected_enum auto ... Values> struct enum_sequence{};
    template<reflected_enum T, typename ISeq> struct make_enum_sequence_impl{};
    template<reflected_enum T, size_t ... Is> struct make_enum_sequence_impl<T, std::index_sequence<Is...>> {
        using type = enum_sequence<enum_values_v<T>[Is]...>;
    };
    template<reflected_enum T> using make_enum_sequence = typename make_enum_sequence_impl<T, std::make_index_sequence<enum_values_v<T>.size()>>::type;

    template<reflected_enum T> struct enum_names {};
    template<reflected_enum T> constexpr auto enum_names_v = enum_names<T>::value;

    template<reflected_enum T> struct enum_full_names {};
    template<reflected_enum T> constexpr auto enum_full_names_v = enum_full_names<T>::value;

    template<reflected_enum T> static constexpr bool is_flags_enum() {
        size_t i = 1;
        for (auto value : enum_values_v<T>) {
            if (value != static_cast<T>(i)) return false;
            i <<= 1;
        }
        return true;
    }
    template<typename T> concept flags_enum = is_flags_enum<T>();

    template<reflected_enum T> static constexpr bool is_linear_enum() {
        size_t i = 1;
        for (auto value : enum_values_v<T>) {
            if (value != static_cast<T>(i)) return false;
            ++i;
        }
        return true;
    }
    template<typename T> concept linear_enum = is_linear_enum<T>();

    template<reflected_enum T> constexpr size_t indexof(T value) {
        if constexpr (flags_enum<T>) {
            size_t i = 0;
            for (size_t n = static_cast<size_t>(value); n != 1; n >>= 1, ++i);
            return i;
        } else if constexpr (linear_enum<T>) {
            return static_cast<size_t>(value);
        } else {
            return std::ranges::find(enum_values_v<T>, value) - enum_values_v<T>.begin();
        }
    }

    template<reflected_enum T> struct enum_data {};
    template<typename T> concept data_enum = requires(T) {
        enum_data<T>::value;
    };
    template<data_enum T> constexpr const auto &get_data(T value) {
        return enum_data<T>::value[indexof(value)];
    };

    template<reflected_enum auto Enum> struct get_type{};
    template<reflected_enum auto Enum> using get_type_t = typename get_type<Enum>::type;

    template<typename T> concept type_enum = reflected_enum<T> && requires {
        typename get_type<enum_values_v<T>[0]>::type;
    };

    template<reflected_enum T> constexpr T invalid_enum_value = static_cast<T>(std::numeric_limits<std::underlying_type_t<T>>::max());

    template<reflected_enum T> constexpr T from_string(std::string_view str) {
        if (auto it = std::ranges::find(enum_names_v<T>, str); it != enum_names_v<T>.end()) {
            return enum_values_v<T>[it - enum_names_v<T>.begin()];
        } else {
            return invalid_enum_value<T>;
        }
    }

    template<reflected_enum T> constexpr std::string_view to_string(T value) {
        return enum_names_v<T>[indexof(value)];
    }
    
    template<reflected_enum T> constexpr std::string_view full_name(T value) {
        return enum_full_names_v<T>[indexof(value)];
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
            for (auto value : enum_values_v<T>) {
                if (flags.check(value)) {
                    out << to_string(value) << ' ';
                }
            }
            return out;
        }
    };
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

#define CREATE_ENUM_VALUES_ELEMENT(r, enumName, elementTuple) (enumName::ENUM_ELEMENT_NAME(elementTuple))
#define CREATE_ENUM_NAMES_ELEMENT(r, enumName, elementTuple) (BOOST_PP_STRINGIZE(ENUM_ELEMENT_NAME(elementTuple)))
#define CREATE_ENUM_FULL_NAMES_ELEMENT(r, enumName, elementTuple) (BOOST_PP_STRINGIZE(enumName) "::" BOOST_PP_STRINGIZE(ENUM_ELEMENT_NAME(elementTuple)))

#define CREATE_ENUM_ELEMENT_MAX_VALUE(n) n - 1
#define CREATE_FLAG_ELEMENT_MAX_VALUE(n) 1 << (n - 1)

#define ENUM_INT(enum_value_fun, elementTupleSeq) enums::sized_int_t<enum_value_fun##_MAX_VALUE(BOOST_PP_SEQ_SIZE(elementTupleSeq))>

#define UNROLL_ELEMENT_VALUE(elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 1), DO_NOTHING, STRIP_PARENS) \
    (STRIP_PARENS_ARGS BOOST_PP_TUPLE_POP_FRONT(elementTuple))

#define GENERATE_CASE_GET_DATA(r, valueType, elementTuple) valueType{ UNROLL_ELEMENT_VALUE(elementTuple) },

#define CREATE_ENUM_GET_DATA(enumName, elementTupleSeq, valueType) \
template<> struct enum_data<enumName> { \
    static inline const std::array value { \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_DATA, valueType, elementTupleSeq) \
    }; \
};

#define GENERATE_GET_TYPE_STRUCT(enumName, elementTuple) \
    template<> struct get_type<enumName::ENUM_ELEMENT_NAME(elementTuple)> { using type = BOOST_PP_TUPLE_ELEM(1, elementTuple); };

#define GENERATE_CASE_GET_TYPE(r, enumName, elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 1), \
        DO_NOTHING, GENERATE_GET_TYPE_STRUCT)(enumName, elementTuple)

#define CREATE_ENUM_GET_TYPE(enumName, elementTupleSeq, ...) \
    template<enumName Enum> struct get_type<Enum> { using type = void; }; \
    BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_TYPE, enumName, elementTupleSeq)

#define GENERATE_ENUM_STRUCTS(enumName, elementTupleSeq) \
template<> struct enum_values<enumName> { \
    static constexpr std::array value { \
        BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_VALUES_ELEMENT, enumName, elementTupleSeq)) \
    }; \
}; \
template<> struct enum_full_names<enumName> { \
    static constexpr std::array<std::string_view, BOOST_PP_SEQ_SIZE(elementTupleSeq)> value { \
        BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_FULL_NAMES_ELEMENT, enumName, elementTupleSeq)) \
    }; \
}; \
template<> struct enum_names<enumName> { \
    static constexpr std::array<std::string_view, BOOST_PP_SEQ_SIZE(elementTupleSeq)> value { \
        BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_NAMES_ELEMENT, enumName, elementTupleSeq)) \
    }; \
};

#define IMPL_DEFINE_ENUM(enumName, elementTupleSeq, enum_value_fun, enum_data_fun, ...) \
enum class enumName : ENUM_INT(enum_value_fun, elementTupleSeq) { \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH_I(enum_value_fun, enumName, elementTupleSeq)) \
}; namespace enums { \
GENERATE_ENUM_STRUCTS(enumName, elementTupleSeq) \
enum_data_fun(enumName, elementTupleSeq, __VA_ARGS__) }

#define IMPL_DEFINE_ENUM_IN_NS(namespaceName, enumName, elementTupleSeq, enum_value_fun, enum_data_fun, ...) \
    enum class enumName : ENUM_INT(enum_value_fun, elementTupleSeq) { \
        BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH_I(enum_value_fun, enumName, elementTupleSeq)) \
    }; \
} namespace enums { using namespace namespaceName; \
GENERATE_ENUM_STRUCTS(namespaceName::enumName, elementTupleSeq) \
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