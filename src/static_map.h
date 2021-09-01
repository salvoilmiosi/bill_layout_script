#ifndef __STATIC_MAP_H__
#define __STATIC_MAP_H__

#include <string>
#include <span>

namespace util {

template<typename Key, typename Value, size_t Size>
class static_map {
private:
    using value_type = std::pair<Key, Value>;
    std::array<value_type, Size> m_data;

public:
    constexpr static_map(std::span<const value_type, Size> data) {
        std::ranges::copy(data, m_data.begin());
        std::ranges::sort(m_data, {}, &value_type::first);
    }

    constexpr auto begin() const { return m_data.begin(); }
    constexpr auto end() const { return m_data.end(); }

    constexpr auto find(const auto &key) const {
        if (auto it = std::ranges::lower_bound(m_data, key, {}, &value_type::first); it->first == key) {
            return it;
        } else {
            return end();
        }
    }
};

template<typename Value, size_t Capacity> using static_string_map = static_map<std::string_view, Value, Capacity>;

}

#endif