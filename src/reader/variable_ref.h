#ifndef __VARIABLE_REF_H__
#define __VARIABLE_REF_H__

#include <string>
#include <map>

#include "variable.h"

template<typename Key, typename Value>
class multimap_range {
public:
    using key_type = Key;
    using value_type = Value;

    using map_type = std::multimap<key_type, value_type>;

private:
    map_type &m_map;
    key_type m_key;

    map_type::iterator m_begin;
    map_type::iterator m_end;

    size_t m_len;

public:
    multimap_range(map_type &map, const key_type &key) : m_map(map), m_key(key) {
        auto range = m_map.equal_range(m_key);
        m_begin = range.first;
        m_end = range.second;
        m_len = std::distance(m_begin, m_end);
    }

    void resize(size_t newlen) {
        while (newlen > m_len) {
            auto it = m_map.emplace_hint(m_end, m_key, value_type());
            if (m_len == 0) {
                m_begin = it;
            }
            m_end = std::next(it);
            ++m_len;
        }
    }

    void clear() {
        m_begin = m_end = m_map.erase(m_begin, m_end);
        m_len = 0;
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

template<typename Key, typename Value>
multimap_range(std::multimap<Key, Value> &, const Value &) -> multimap_range<Key, Value>;

enum set_flags {
    SET_ASSIGN = 0,
    SET_RESET = 1 << 0,
    SET_INCREASE = 1 << 1
};

class variable_ref : public multimap_range<std::string, variable> {
public:
    size_t index = 0;
    size_t range_len = 0;

public:
    variable_ref(map_type &map, const std::string &key, size_t index = 0, size_t range_len = 0) :
        multimap_range(map, key), index(index), range_len(range_len) {}

public:
    const variable &get_value() const {
        if (index < size()) {
            return std::next(begin(), index)->second;
        } else {
            return variable::null_var();
        }
    }

    variable get_moved() {
        if (index < size()) {
            return std::move(std::next(begin(), index)->second);
        } else {
            return variable();
        }
    }

    void set_value(variable &&value, set_flags flags = SET_ASSIGN) {
        if (value.empty()) return;

        if (flags & SET_RESET) clear();

        resize(index + range_len);

        auto it = std::next(begin(), index);

        if (flags & SET_INCREASE) {
            std::for_each_n(it, range_len, [&](auto &var) {
                var.second += value;
            });
        } else if (range_len == 1) {
            it->second = std::move(value);
        } else {
            std::for_each_n(it, range_len, [&](auto &var) {
                var.second = value;
            });
        }
    }
};

#endif