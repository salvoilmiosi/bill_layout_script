#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <boost/preprocessor.hpp>

#include "utils.h"

template<typename T> struct EnumSizeImpl {};
template<typename T> constexpr T FindEnum(std::string_view name) = delete;
template<typename T> constexpr size_t EnumSize = EnumSizeImpl<T>::value;

typedef uint8_t flags_t;

template<typename T>
concept string_enum = std::is_enum_v<T> && requires (T x) {
    { ToString(x) } -> std::convertible_to<const char *>;
    { EnumSize<T> } -> std::convertible_to<size_t>;
};

template<string_enum T> struct IsFlagEnum : std::false_type {};
template<typename T> concept flags_enum = IsFlagEnum<T>::value;

template<typename T, string_enum E> constexpr T EnumData(E x) { return std::get<T>(GetTuple(x)); }
template<size_t I, string_enum E> constexpr auto EnumData(E x) { return std::get<I>(GetTuple(x)); }

#define HELPER1(...) ((__VA_ARGS__)) HELPER2
#define HELPER2(...) ((__VA_ARGS__)) HELPER1
#define HELPER1_END
#define HELPER2_END
#define ADD_PARENTHESES(sequence) BOOST_PP_CAT(HELPER1 sequence,_END)

#define GET_FIRST_OF(element, ...) element
#define GET_TAIL_OF(element, ...) __VA_ARGS__

#define CREATE_ENUM_ELEMENT(r, data, elementTuple) (GET_FIRST_OF elementTuple)
#define CREATE_FLAG_ELEMENT(r, data, i, elementTuple) (GET_FIRST_OF elementTuple = 1 << i)

#define GENERATE_CASE_TO_STRING(r, enumName, elementTuple) \
    case enumName::GET_FIRST_OF elementTuple : return BOOST_PP_STRINGIZE(GET_FIRST_OF elementTuple);

#define GENERATE_CASE_FIND_ENUM(r, enumName, elementTuple) \
    case hash(BOOST_PP_STRINGIZE(GET_FIRST_OF elementTuple)): return enumName::GET_FIRST_OF elementTuple;

#define GENERATE_CASE_GET_DATA(r, enumName, elementTuple) \
    case enumName::GET_FIRST_OF elementTuple : return std::make_tuple(GET_TAIL_OF elementTuple);

#define GENERATE_DEFAULT_CASE(enumName) \
    default: throw std::invalid_argument("[Unknown " BOOST_PP_STRINGIZE(enumName) "]");

#define DEFINE_ENUM_FUNCTIONS(enumName, enumElementsParen) \
template<> struct EnumSizeImpl<enumName> : std::integral_constant<size_t, BOOST_PP_SEQ_SIZE(enumElementsParen)> {}; \
constexpr const char *ToString(const enumName element) { \
    switch (element) { \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_TO_STRING, enumName, enumElementsParen) \
        GENERATE_DEFAULT_CASE(enumName) \
    } \
} \
template<> constexpr enumName FindEnum(std::string_view name) { \
    switch (hash(name)) { \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_FIND_ENUM, enumName, enumElementsParen) \
        GENERATE_DEFAULT_CASE(enumName) \
    } \
}

#define DEFINE_ENUM_GET_TUPLE(enumName, enumElementsParen) \
constexpr auto GetTuple(const enumName element) { \
    switch (element) { \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_DATA, enumName, enumElementsParen) \
        GENERATE_DEFAULT_CASE(enumName) \
    } \
}

#define GENERATE_CASE_GET_TYPE(enumName, element, elementType) \
    template<> struct EnumTypeImpl<enumName::element> { using type = elementType; };
#define GENERATE_TYPE_STRUCT(r, enumName, elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 2), \
        GENERATE_CASE_GET_TYPE(enumName, GET_FIRST_OF elementTuple, GET_TAIL_OF elementTuple), BOOST_PP_EMPTY())

#define DEFINE_ENUM_GET_TYPE(enumName, enumElementsParen) \
    template<enumName Enum> struct EnumTypeImpl{ using type = void; }; \
    BOOST_PP_SEQ_FOR_EACH(GENERATE_TYPE_STRUCT, enumName, enumElementsParen) \
    template<enumName Enum> using EnumType = typename EnumTypeImpl<Enum>::type;

#define DEFINE_ENUM(enumName, enumElements) \
enum class enumName : uint8_t { \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_ELEMENT, enumName, ADD_PARENTHESES(enumElements))) \
}; \
DEFINE_ENUM_FUNCTIONS(enumName, ADD_PARENTHESES(enumElements)) \
DEFINE_ENUM_GET_TUPLE(enumName, ADD_PARENTHESES(enumElements))

#define DEFINE_ENUM_FLAGS(enumName, enumElements) \
enum class enumName : flags_t { \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH_I(CREATE_FLAG_ELEMENT, enumName, ADD_PARENTHESES(enumElements))) \
}; \
DEFINE_ENUM_FUNCTIONS(enumName, ADD_PARENTHESES(enumElements)) \
DEFINE_ENUM_GET_TUPLE(enumName, ADD_PARENTHESES(enumElements)) \
template<> struct IsFlagEnum<enumName> : std::true_type {};

#define DEFINE_ENUM_TYPES(enumName, enumElements) \
enum class enumName : uint8_t { \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_ELEMENT, enumName, ADD_PARENTHESES(enumElements))) \
}; \
DEFINE_ENUM_FUNCTIONS(enumName, ADD_PARENTHESES(enumElements)) \
DEFINE_ENUM_GET_TYPE(enumName, ADD_PARENTHESES(enumElements))

template<flags_enum T>
class bitset {
private:
    flags_t m_value = 0;

public:
    constexpr bitset() = default;
    constexpr bitset(flags_t value) : m_value(value) {}

    constexpr friend bitset operator & (bitset lhs, T rhs) { return lhs.m_value & flags_t(rhs); }
    constexpr friend bitset operator & (T lhs, bitset rhs) { return flags_t(lhs) & rhs.m_value; }
    constexpr friend bitset operator | (bitset lhs, T rhs) { return lhs.m_value | flags_t(rhs); }
    constexpr friend bitset operator | (T lhs, bitset rhs) { return flags_t(lhs) | rhs.m_value; }
    constexpr friend bitset operator ^ (bitset lhs, T rhs) { return lhs.m_value ^ flags_t(rhs); }
    constexpr friend bitset operator ^ (T lhs, bitset rhs) { return flags_t(lhs) ^ rhs.m_value; }
    
    constexpr bitset operator ~() const { return ~m_value; }
 
    constexpr bitset operator << (auto n) const { return m_value << n; }
    constexpr bitset operator >> (auto n) const { return m_value >> n; }
 
    constexpr bitset &operator &= (auto n) { m_value &= flags_t(n); return *this; }
    constexpr bitset &operator |= (auto n) { m_value |= flags_t(n); return *this; }
    constexpr bitset &operator ^= (auto n) { m_value ^= flags_t(n); return *this; }
    constexpr bitset &operator <<= (auto n) { m_value <<= flags_t(n); return *this; }
    constexpr bitset &operator >>= (auto n) { m_value >>= flags_t(n); return *this; }
 
    constexpr operator flags_t() const { return m_value; }
};

template<string_enum T>
std::ostream &operator << (std::ostream &out, T value) {
    return out << ToString(value);
}

template<flags_enum T>
std::ostream &operator << (std::ostream &out, const bitset<T> &flags) {
    for (size_t i=0; i < EnumSize<T>; ++i) {
        if (flags & (1 << i)) {
            out << static_cast<T>(1 << i) << ' ';
        }
    }
    return out;
}

#endif