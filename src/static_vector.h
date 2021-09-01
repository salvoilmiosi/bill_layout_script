#ifndef __STATIC_VECTOR_H__
#define __STATIC_VECTOR_H__

#include <stdexcept>
#include <array>

namespace util {
    template<typename T, size_t N> class static_vector {
    private:
        std::array<T, N> m_data;
        size_t m_size = 0;

    public:
        constexpr static_vector() = default;
        
        constexpr static_vector(std::initializer_list<T> init) {
            for (auto &obj : init) {
                push_back(obj);
            }
        }

        template<typename ... Ts>
        constexpr T &emplace_back(Ts && ... args) {
            if (m_size >= N) {
                throw std::out_of_range("Stack overflow");
            }
            m_data[m_size++] = T{std::forward<Ts>(args) ... };
            return back();
        }

        constexpr void push_back(const T &obj) { emplace_back(obj); }
        constexpr void push_back(T &&obj) { emplace_back(std::move(obj)); }

        constexpr void pop_back() {
            if (m_size == 0) {
                throw std::out_of_range("Stack underflow");
            }
            --m_size;
            delete m_data[m_size].data();
        }

        constexpr void clear() noexcept {
            m_size = 0;
        }

        constexpr bool empty() const noexcept {
            return m_size == 0;
        }

        constexpr size_t size() const noexcept {
            return m_size;
        }

        constexpr static size_t capacity() {
            return N;
        }

        constexpr T &back() noexcept {
            return (*this)[m_size - 1];
        }

        constexpr const T &back() const noexcept {
            return (*this)[m_size - 1];
        }

        constexpr T &operator[](size_t index) {
            return m_data[index];
        }

        constexpr const T &operator[](size_t index) const {
            return m_data[index];
        }

        constexpr const T *begin() const { return m_data.data(); }
        constexpr const T *end() const { return m_data.data() + m_size; }

        constexpr T *begin() { return m_data.data(); }
        constexpr T *end() { return m_data.data() + m_size; }

        constexpr const auto rbegin() const { return std::reverse_iterator(end()); }
        constexpr const auto rend() const { return std::reverse_iterator(begin()); }

        constexpr auto rbegin() { return std::reverse_iterator(end()); }
        constexpr auto rend() { return std::reverse_iterator(begin()); }
    };

}

#endif