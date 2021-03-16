#ifndef __STACK_H__
#define __STACK_H__

#include <vector>

constexpr size_t MAX_SIZE = 8;
constexpr size_t DEFAULT_SIZE = 32;

template<typename T, size_t N = DEFAULT_SIZE>
struct auto_reserve_vector : std::vector<T> {
    using base = std::vector<T>;
    auto_reserve_vector() {
        base::reserve(N);
    }
};

template<typename T, typename Container = auto_reserve_vector<T>> struct simple_stack : public Container {
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

    constexpr T pop() {
        T ret = std::move(base::back());
        base::pop_back();
        return ret;
    }
};

template<typename T, size_t N = MAX_SIZE> class static_vector {
private:
    T m_data[N];
    size_t m_size = 0;

public:
    template<typename U>
    void push_back(U &&obj) {
        if (m_size >= N) {
            throw std::out_of_range("Stack overflow");
        }
        m_data[m_size++] = std::forward<U>(obj);
    }

    template<typename ... Ts>
    T &emplace_back(Ts && ... args) {
        push_back(T{std::forward<Ts>(args) ... });
        return back();
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