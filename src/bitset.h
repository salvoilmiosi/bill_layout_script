#ifndef __BITSET_H__
#define __BITSET_H__

#include <cstdint>
#include <concepts>

typedef uint8_t flags_t;

template<typename T>
concept flag_enum = requires (T x) {
    flags_t(x);
};

template<flag_enum T>
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

template<flag_enum T>
class print_flags {
private:
    bitset<T> m_flags;
    const char **m_names;
    size_t m_num;

public:
    template<size_t N>
    print_flags(bitset<T> flags, const char * (&names)[N])
        : m_flags(flags), m_names(names), m_num(N) {}

    friend std::ostream &operator << (std::ostream &out, const print_flags &printer) {
        for (size_t i=0; i<printer.m_num; ++i) {
            if (printer.m_flags & (1 << i)) {
                out << ' ' << printer.m_names[i];
            }
        }
        return out;
    }
};

#endif