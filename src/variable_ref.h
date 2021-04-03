#ifndef __VARIABLE_REF_H__
#define __VARIABLE_REF_H__

#include <string>
#include <map>

#include "variable.h"
#include "bytecode.h"

template<typename Map>
class multimap_range {
public:
    using key_type = typename Map::key_type;
    using value_type = typename Map::mapped_type;

    using map_type = Map;
    using iterator_type = typename Map::iterator;

private:
    map_type *m_map = nullptr;
    key_type m_key;

    iterator_type m_begin;
    iterator_type m_end;

    size_t m_len;

public:
    multimap_range() = default;

    multimap_range(map_type &map, const key_type &key) : m_map(&map), m_key(key) {
        auto range = m_map->equal_range(m_key);
        m_begin = range.first;
        m_end = range.second;
        m_len = std::distance(m_begin, m_end);
    }

    void resize(size_t newlen) {
        while (newlen > m_len) {
            auto it = m_map->emplace_hint(m_end, m_key, value_type());
            if (m_len == 0) {
                m_begin = it;
            }
            m_end = std::next(it);
            ++m_len;
        }
    }

    void clear() {
        m_begin = m_end = m_map->erase(m_begin, m_end);
        m_len = 0;
    }

    value_type &at(size_t index) const {
        if (index < size()) {
            return std::next(begin(), index)->second;
        } else {
            throw std::out_of_range("Out of range");
        }
    }

    size_t size() const {
        return m_len;
    }

    auto begin() const {
        return m_begin;
    }

    auto end() const {
        return m_end;
    }
};

struct variable_key {
    std::string name;
    small_int table_index;

    static constexpr small_int global_index = -1;

    inline bool operator == (const variable_key &other) const = default;

    inline auto operator <=> (const variable_key &other) const {
        if (table_index == other.table_index) {
            return name <=> other.name;
        } else {
            return table_index <=> other.table_index;
        }
    };
};

using variable_map = std::multimap<variable_key, variable>;

class variable_ref : public multimap_range<variable_map> {
public:
    size_t index = 0;
    size_t length = 0;

public:
    variable_ref() = default;

    variable_ref(map_type &map, const key_type &key, size_t index = 0, size_t length = 0) :
        multimap_range(map, key), index(index), length(length) {}

public:
    const variable &get_value() const {
        static const variable null_var;
        if (index < size()) {
            return std::next(begin(), index)->second;
        } else {
            return null_var;
        }
    }

    void set_value(variable &&value, bitset<setvar_flags> flags) {
        if (!(flags & setvar_flags::FORCE) && value.is_null()) return;
        if (flags & setvar_flags::OVERWRITE) clear();

        resize(index + length);

        auto it = std::next(begin(), index);

        if (flags & setvar_flags::INCREASE) {
            std::for_each_n(it, length, [&](auto &var) {
                var.second += value;
            });
        } else {
            for (int n = length; n > 1; ++it, --n) {
                it->second.assign(value);
            }
            it->second.assign(std::move(value));
        }
    }
};

#endif