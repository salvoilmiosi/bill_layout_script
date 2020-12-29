#ifndef __STACK_H__
#define __STACK_H__

#include <vector>
template<typename T, typename Container = std::vector<T>> struct my_stack : public Container {
    using base = Container;
    
    T &top() { return base::back(); }
    const T &top() const { return base::back(); }

    void push(const T &value) { base::push_back(value); }
    void push(T &&value) { base::push_back(std::move(value)); }

    void pop() { base::pop_back(); }
};

#endif