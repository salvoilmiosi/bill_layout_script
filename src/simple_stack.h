#ifndef __SIMPLE_STACK_H__
#define __SIMPLE_STACK_H__

#include <deque>

namespace util {
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
}

#endif