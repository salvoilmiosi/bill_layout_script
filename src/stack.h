#ifndef __STACK_H__
#define __STACK_H__

#include <vector>
template<typename T, typename Container = std::vector<T>> struct simple_stack : public Container {
    using base = Container;
    
    constexpr T &top() { return base::back(); }
    constexpr const T &top() const { return base::back(); }

    constexpr void push(auto &&value) {
        base::push_back(std::forward<decltype(value)>(value));
    }

    constexpr void pop() {
        base::pop_back();
    }
};

constexpr size_t MAX_SIZE = 8;

template<typename T, size_t N = MAX_SIZE> class static_vector {
private:
    T m_data[N];
    size_t m_size = 0;

public:
    void push_back(auto &&obj) {
        if (m_size >= N) {
            throw std::out_of_range("Stack overflow");
        }
        m_data[m_size++] = std::forward<decltype(obj)>(obj);
    }

    template<typename ... Ts>
    void emplace_back(Ts && ... args) {
        push_back(T(std::forward<Ts>(args) ...));
    }

    void pop_back() {
        if (m_size == 0) {
            throw std::out_of_range("Stack underflow");
        }
        --m_size;
    }

    void clear() {
        m_size = 0;
    }

    bool empty() {
        return m_size == 0;
    }

    size_t size() {
        return m_size;
    }

    size_t capacity() {
        return N;
    }

    T &back() {
        return (*this)[m_size - 1];
    }

    const T &back() const {
        return (*this)[m_size - 1];
    }

    T &operator[](size_t index) {
        return m_data[index];
    }

    const T &operator[](size_t index) const {
        return m_data[index];
    }
};

template<typename T, size_t N = MAX_SIZE>
using static_stack = simple_stack<T, static_vector<T, N>>;

#endif