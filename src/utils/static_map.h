#ifndef __STATIC_MAP_H__
#define __STATIC_MAP_H__

#include <stdexcept>
#include <string>
#include <span>

namespace util {

    namespace impl {
        template<typename Key, typename Value, size_t Size>
        class static_map {
        private:
            using value_type = std::pair<Key, Value>;
            std::array<value_type, Size> m_data;

        public:
            constexpr static_map(value_type (&&data)[Size]) {
                std::ranges::move(data, m_data.begin());
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

        template<typename T, typename U>
        concept castable_to = requires(T t) {
            static_cast<U>(t);
        };

        template<castable_to<size_t> Key, typename Value, size_t Size>
        class contig_static_map {
        private:
            using value_type = std::pair<Key, Value>;
            std::array<value_type, Size> m_data;

        public:
            constexpr contig_static_map(value_type (&&data)[Size]) {
                std::ranges::move(data, m_data.begin());
                size_t n = static_cast<size_t>(m_data.front().first);
                for (const auto &i : m_data) {
                    if (n != static_cast<size_t>(i.first)) throw std::logic_error("Keys must be contiguous");
                    ++n;
                }
            }

            constexpr auto begin() const { return m_data.begin(); }
            constexpr auto end() const { return m_data.end(); }

            constexpr auto find(const Key &key) const {
                const size_t ikey = static_cast<size_t>(key);
                const size_t ifirst = static_cast<size_t>(m_data.front().first);
                const size_t ilast = static_cast<size_t>(m_data.back().first);
                if (ikey >= ifirst && ikey <= ilast) {
                    return begin() + (ikey - ifirst);
                } else {
                    return end();
                }
            }
        };
    }

    template<typename Key, typename Value, size_t Size>
    constexpr auto static_map(std::pair<Key, Value> (&&data)[Size]) {
        return impl::static_map<Key, Value, Size>(std::move(data));
    }

    template<impl::castable_to<size_t> Key, typename Value, size_t Size>
    constexpr auto contig_static_map(std::pair<Key, Value> (&&data)[Size]) {
        return impl::contig_static_map<Key, Value, Size>(std::move(data));
    }

}

#endif