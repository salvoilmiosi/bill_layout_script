#ifndef __BITSET_H__
#define __BITSET_H__

#include <cstdint>
#include <boost/preprocessor.hpp>
#include <concepts>

struct hasher {
    constexpr size_t operator() (const char *begin, const char *end) const {
        return begin != end ? static_cast<unsigned int>(*begin) + 33 * (*this)(begin + 1, end) : 5381;
    }
    
    constexpr size_t operator() (std::string_view str) const {
        return (*this)(str.begin(), str.end());
    }
};

// restituisce l'hash di una stringa
template<typename T> size_t constexpr hash(T&& t) {
    return hasher{}(std::forward<T>(t));
}

template<typename T> struct EnumSizeImpl {};
template<typename T> constexpr T FindEnum(std::string_view name) = delete;
template<typename T> constexpr size_t EnumSize = EnumSizeImpl<T>::value;

typedef uint8_t flags_t;

template<typename T>
concept string_enum = requires (T x) {
    { ToString(x) } -> std::convertible_to<const char *>;
    { EnumSize<T> } -> std::convertible_to<size_t>;
    GetTuple(x);
};

template<typename T, string_enum E> constexpr T EnumData(E x) { return std::get<T>(GetTuple(x)); }
template<size_t I, string_enum E> constexpr auto EnumData(E x) { return std::get<I>(GetTuple(x)); }

#define HELPER1(...) ((__VA_ARGS__)) HELPER2
#define HELPER2(...) ((__VA_ARGS__)) HELPER1
#define HELPER1_END
#define HELPER2_END
#define ADD_PARENTHESES_FOR_EACH_TUPLE_IN_SEQ(sequence) BOOST_PP_CAT(HELPER1 sequence,_END)

#define CREATE_ENUM_ELEMENT(r, data, elementTuple) BOOST_PP_TUPLE_ELEM(0, elementTuple),
#define CREATE_FLAG_ELEMENT(r, data, i, elementTuple) BOOST_PP_TUPLE_ELEM(0, elementTuple) = (1 << i),

#define GENERATE_CASE_TO_STRING_IMPL(enumName, element) \
    case enumName::element : return BOOST_PP_STRINGIZE(element);

#define GENERATE_CASE_TO_STRING(r, enumName, elementTuple) \
    GENERATE_CASE_TO_STRING_IMPL(enumName, BOOST_PP_TUPLE_ELEM(0, elementTuple))

#define GENERATE_CASE_FIND_ENUM_IMPL(enumName, element) \
    case hash(BOOST_PP_STRINGIZE(element)): return enumName::element;

#define GENERATE_CASE_FIND_ENUM(r, enumName, elementTuple) \
    GENERATE_CASE_FIND_ENUM_IMPL(enumName, BOOST_PP_TUPLE_ELEM(0, elementTuple))

#define GENERATE_CASE_GET_DATA_RETURN_VALUE(enumName, element, valueTuple) \
    case enumName::element : return std::make_tuple valueTuple;

#define GENERATE_CASE_GET_DATA_NO_RETURN(enumName, element) \
    case enumName::element : return;

#define GENERATE_CASE_GET_DATA_IMPL(enumName, elementTuple) \
    BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_TUPLE_SIZE(elementTuple), 1),   \
        GENERATE_CASE_GET_DATA_NO_RETURN(enumName, BOOST_PP_TUPLE_ELEM(0, elementTuple)), \
        GENERATE_CASE_GET_DATA_RETURN_VALUE(enumName, BOOST_PP_TUPLE_ELEM(0, elementTuple), BOOST_PP_TUPLE_POP_FRONT(elementTuple)))

#define GENERATE_CASE_GET_DATA(r, enumName, elementTuple) \
    GENERATE_CASE_GET_DATA_IMPL(enumName, elementTuple)

#define DEFINE_ENUM_FUNCTIONS(enumName, enumElementsParen)  \
template<> struct EnumSizeImpl<enumName> : std::integral_constant<size_t, BOOST_PP_SEQ_SIZE(enumElementsParen)> {}; \
constexpr const char *ToString(const enumName element) {                           \
    switch (element) {                                                      \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_TO_STRING, enumName, enumElementsParen)  \
        default: throw std::invalid_argument("[Unknown " BOOST_PP_STRINGIZE(enumName) "]"); \
    }                                                                       \
}               \
template<> constexpr enumName FindEnum(std::string_view name) {     \
    switch (hash(name)) {       \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_FIND_ENUM, enumName, enumElementsParen) \
        default: throw std::invalid_argument("[Unknown " BOOST_PP_STRINGIZE(enumName) "]"); \
    }   \
} \
constexpr auto GetTuple(const enumName element) {    \
    switch (element) {  \
        BOOST_PP_SEQ_FOR_EACH(GENERATE_CASE_GET_DATA, enumName, enumElementsParen)    \
        default: throw std::invalid_argument("[Unknown " BOOST_PP_STRINGIZE(enumName) "]"); \
    }   \
}

#define DEFINE_ENUM(enumName, enumElements)          \
enum class enumName : uint8_t { \
    BOOST_PP_SEQ_FOR_EACH(CREATE_ENUM_ELEMENT, enumName, ADD_PARENTHESES_FOR_EACH_TUPLE_IN_SEQ(enumElements)) \
}; DEFINE_ENUM_FUNCTIONS(enumName, ADD_PARENTHESES_FOR_EACH_TUPLE_IN_SEQ(enumElements))

#define DEFINE_ENUM_FLAGS(enumName, enumElements) \
enum class enumName : flags_t { \
    BOOST_PP_SEQ_FOR_EACH_I(CREATE_FLAG_ELEMENT, enumName, ADD_PARENTHESES_FOR_EACH_TUPLE_IN_SEQ(enumElements))    \
}; DEFINE_ENUM_FUNCTIONS(enumName, ADD_PARENTHESES_FOR_EACH_TUPLE_IN_SEQ(enumElements))

template<typename T>
class bitset {
private:
    flags_t m_value = 0;

public:
    bitset() = default;
    bitset(flags_t value) : m_value(value) {}

    friend bitset operator & (bitset lhs, T rhs) {
        return lhs.m_value & flags_t(rhs);
    }
    friend bitset operator & (T lhs, bitset rhs) {
        return flags_t(lhs) & rhs.m_value;
    }
    friend bitset operator | (bitset lhs, T rhs) {
        return lhs.m_value | flags_t(rhs);
    }
    friend bitset operator | (T lhs, bitset rhs) {
        return flags_t(lhs) | rhs.m_value;
    }
    friend bitset operator ^ (bitset lhs, T rhs) {
        return lhs.m_value ^ flags_t(rhs);
    }
    friend bitset operator ^ (T lhs, bitset rhs) {
        return flags_t(lhs) ^ rhs.m_value;
    }
    bitset operator ~() const {
        return ~m_value;
    }
    bitset operator << (auto n) const {
        return m_value << n;
    }
    bitset operator >> (auto n) const {
        return m_value >> n;
    }
    bitset &operator &= (auto n) {
        m_value &= flags_t(n);
        return *this;
    }
    bitset &operator |= (auto n) {
        m_value |= flags_t(n);
        return *this;
    }
    bitset &operator <<= (auto n) {
        m_value <<= flags_t(n);
        return *this;
    }
    bitset &operator >>= (auto n) {
        m_value >>= flags_t(n);
        return *this;
    }
    operator flags_t() const {
        return m_value;
    }
};

template<string_enum T>
std::ostream &operator << (std::ostream &out, const bitset<T> &flags) {
    for (size_t i=0; i < EnumSize<T>; ++i) {
        if (flags & (1 << i)) {
            out << ' ' << ToString(static_cast<T>(1 << i));
        }
    }
    return out;
}

#endif