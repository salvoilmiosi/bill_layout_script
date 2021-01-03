#include "reader.h"

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

public:
    multimap_range(map_type &map, const key_type &key) : m_map(map), m_key(key) {
        auto range = m_map.equal_range(key);
        m_begin = range.first;
        m_end = range.second;
    }

    void resize(size_t newlen) {
        while (newlen > size()) {
            auto it = m_map.insert({m_key, value_type()});
            if (m_begin == m_end) {
                m_begin = it;
            }
            m_end = std::next(it);
        }
    }

    void clear() {
        m_map.erase(m_begin, m_end);
        m_begin = m_end = m_map.end();
    }

    size_t size() {
        return std::distance(m_begin, m_end);
    }

    auto begin() {
        return m_begin;
    }

    auto end() {
        return m_end;
    }

    variable &operator[] (size_t index) {
        return std::next(m_begin, index)->second;
    }
};

template<typename Key, typename Value>
multimap_range(std::multimap<Key, Value> &, const Value &) -> multimap_range<Key, Value>;

void variable_ref::set_value(variable &&value, set_flags flags) {
    if (value.empty()) return;

    multimap_range var(parent.get_page(page_idx), name);

    if (flags & SET_RESET) var.clear();

    var.resize(index + range_len);

    auto it_begin = std::next(var.begin(), index);
    auto it_end = std::next(it_begin, range_len);

    if (flags & SET_INCREASE) {
        std::for_each(it_begin, it_end, [&](auto &var) {
            var.second += value;
        });
    } else if (range_len == 1) {
        var[index] = std::move(value);
    } else {
        std::for_each(it_begin, it_end, [&](auto &var) {
            var.second = value;
        });
    }
}

const variable &variable_ref::get_value() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        multimap_range var(parent.get_page(page_idx), name);
        if (index < var.size()) {
            return var[index];
        }
    }
    return variable::null_var();
}

void variable_ref::clear() {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        parent.get_page(page_idx).erase(name);
    }
}

size_t variable_ref::size() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        return multimap_range(parent.get_page(page_idx), name).size();
    }
    return 0;
}

bool variable_ref::isset() const {
    if (page_idx == PAGE_GLOBAL || page_idx < parent.m_pages.size()) {
        return parent.get_page(page_idx).contains(name);
    }
    return false;
}