#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <span>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {

    struct as_array_tag_t {};
    constexpr as_array_tag_t as_array_tag;

    class content_view {
    private:
        const variable m_data;

        std::span<const variable> m_span;
        std::span<const variable>::iterator m_current;

    public:
        content_view(variable &&var)
            : m_data(std::move(var))
            , m_span(&m_data, 1)
            , m_current(m_span.begin()) {}

        content_view(variable &&var, as_array_tag_t)
            : m_data(std::move(var))
            , m_span(m_data.is_array()
                ? std::span(m_data.as_array())
                : std::span(&m_data, 1))
            , m_current(m_span.begin()) {}

        size_t index() const {
            return m_current - m_span.begin();
        }

        void nextresult() {
            ++m_current;
        }

        bool tokenend() const {
            return m_current >= m_span.end();
        }

        variable view() const {
            if (tokenend()) return {};
            return m_current->as_pointer();
        }
    };

}

#endif