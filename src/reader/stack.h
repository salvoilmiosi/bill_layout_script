#ifndef __STACK_H__
#define __STACK_H__

#include <vector>
template<typename T, typename Container = std::vector<T>> struct my_stack : public Container {
    using base = Container;
    
    constexpr T &top() { return base::back(); }
    constexpr const T &top() const { return base::back(); }

    constexpr void push(const T &value) { base::push_back(value); }
    constexpr void push(T &&value) { base::push_back(std::move(value)); }

    constexpr void pop() { base::pop_back(); }
};

#endif