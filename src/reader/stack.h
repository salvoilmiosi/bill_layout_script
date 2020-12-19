#ifndef __STACK_H__
#define __STACK_H__

#include <vector>
template<typename T, typename Container = std::vector<T>> struct my_stack : public Container {
    using base = Container;

    struct range {
        base::iterator _begin, _end;

        base::iterator &begin() { return _begin; }
        base::iterator &end() { return _end; }

        size_t size() { return _end - _begin; }

        base::value_type &operator[](size_t index) {
            return *(begin() + index);
        }
    };
    
    T &top() { return base::back(); }
    const T &top() const { return base::back(); }

    range top_view(size_t numvars) {
        return {base::end() - numvars, base::end()};
    }

    void push(const T &value) { base::push_back(value); }
    void push(T &&value) { base::push_back(std::move(value)); }

    void pop() { base::pop_back(); }
};

#endif