#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

struct view_span {
    size_t m_begin;
    size_t m_end;
    
    constexpr size_t size() const noexcept {
        return m_end - m_begin;
    }
};

class content_view {
private:
    variable m_value;
    static_stack<view_span> m_spans;

public:
    content_view(auto &&value) : m_value(std::forward<decltype(value)>(value)) {
        m_spans.push(view_span{0, m_value.str_view().size()});
    }

    void setbegin(size_t n) {
        m_spans.top().m_begin += n;
    }

    void setend(size_t n) {
        if (n < m_spans.top().size()) {
            m_spans.top().m_end = m_spans.top().m_begin + n;
        }
    }

    void newview() {
        m_spans.push(m_spans.top());
    }

    void splitview() {
        m_spans.push(view_span{
            m_spans.top().m_begin,
            std::min(
                m_value.str_view().find(RESULT_SEPARATOR, m_spans.top().m_begin),
                m_spans.top().m_end)
        });
    }

    void resetview() {
        assert(m_spans.size() > 1);
        m_spans.pop();
    }
    
    void nextresult() {
        assert(m_spans.size() > 1);
        m_spans.top().m_begin = m_spans.top().m_end + 1;
        if (tokenend()) {
            m_spans.top().m_begin = m_spans.top().m_end = m_spans[m_spans.size() - 2].m_end;
        } else {
            m_spans.top().m_end = std::min(
                m_value.str_view().find(RESULT_SEPARATOR, m_spans.top().m_begin),
                m_spans[m_spans.size() - 2].m_end);
        }
    }

    bool tokenend() {
        assert(m_spans.size() > 1);
        return m_spans.top().m_begin >= m_spans[m_spans.size() - 2].m_end;
    }

    std::string_view view() const {
        return m_value.str_view().substr(m_spans.top().m_begin, m_spans.top().size());
    }
};

#endif