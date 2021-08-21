#ifndef __STACK_H__
#define __STACK_H__

#include <deque>
#include <stdexcept>

namespace bls {

    constexpr size_t default_stack_size = 8;

    template<typename Container> class back_popper {
    private:
        Container *m_container;

    public:
        back_popper(Container *container) : m_container(container) {}
        ~back_popper() { m_container->pop_back(); }

        auto &operator *() { return m_container->back(); }
        const auto &operator *() const { return m_container->back(); }

        auto *operator ->() { return &m_container->back(); }
        const auto *operator ->() const { return &m_container->back(); }
    };

    template<typename T, typename Container = std::deque<T>> struct simple_stack : public Container {
        using base = Container;
        
        constexpr T &top() { return base::back(); }
        constexpr const T &top() const { return base::back(); }

        template<typename U>
        constexpr void push(U &&value) {
            base::push_back(std::forward<U>(value));
        }

        template<typename ... Ts>
        constexpr T &emplace(Ts && ... values) {
            return base::emplace_back(std::forward<Ts>(values) ...);
        }

        auto pop() {
            return back_popper(this);
        }
    };

    template<typename T, size_t N = default_stack_size> class static_vector {
    private:
        T m_data[N];
        size_t m_size = 0;

    public:
        constexpr static_vector() = default;
        
        constexpr static_vector(std::initializer_list<T> init) {
            for (auto &obj : init) {
                push_back(obj);
            }
        }

        template<typename U>
        constexpr void push_back(U &&obj) {
            if (m_size >= N) {
                throw std::out_of_range("Stack overflow");
            }
            m_data[m_size++] = std::forward<U>(obj);
        }

        template<typename ... Ts>
        constexpr T &emplace_back(Ts && ... args) {
            push_back(T{std::forward<Ts>(args) ... });
            return back();
        }

        constexpr void pop_back() {
            if (m_size == 0) {
                throw std::out_of_range("Stack underflow");
            }
            --m_size;
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

        constexpr size_t capacity() const noexcept {
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

        constexpr const T *begin() const {
            return m_data;
        }

        constexpr const T *end() const {
            return m_data + m_size;
        }
    };

    template<typename T> static_vector(std::initializer_list<T>) -> static_vector<T>;

    template<typename T, size_t N = default_stack_size>
    using static_stack = simple_stack<T, static_vector<T, N>>;

}

#endif