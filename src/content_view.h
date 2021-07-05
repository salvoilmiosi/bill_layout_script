#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {

    struct view_span {
        size_t m_begin;
        size_t m_end;
        int m_index;
        
        constexpr size_t size() const noexcept {
            return m_end - m_begin;
        }
    };

    class content_view : private variable {
    private:
        static_stack<view_span> m_spans;

    public:
        template<typename T>
        content_view(T &&value) : variable(std::forward<T>(value)) {
            m_spans.push(view_span{0, as_view().size(), 0});
        }

        void setbegin(size_t n) noexcept {
            m_spans.top().m_begin = std::min(
                m_spans.top().m_begin + n,
                m_spans.top().m_end);
        }

        void setend(size_t n) noexcept {
            m_spans.top().m_end = std::min(
                m_spans.top().m_begin + n,
                m_spans.top().m_end);
        }

        void newview() {
            m_spans.push(m_spans.top());
        }

        void splitview() {
            m_spans.emplace(
                m_spans.top().m_begin,
                std::min(
                    as_view().find(util::unit_separator, m_spans.top().m_begin),
                    m_spans.top().m_end),
                0
            );
        }

        void resetview() noexcept {
            assert(m_spans.size() > 1);
            m_spans.pop();
        }
        
        void nextresult() noexcept {
            assert(m_spans.size() > 1);
            m_spans.top().m_begin = std::min(
                m_spans.top().m_end + 1,
                m_spans[m_spans.size() - 2].m_end);
            m_spans.top().m_end = std::min(
                as_view().find(util::unit_separator, m_spans.top().m_begin),
                m_spans[m_spans.size() - 2].m_end);
            ++m_spans.top().m_index;
        }

        int tokenidx() const noexcept {
            return m_spans.top().m_index;
        }

        bool tokenend() const noexcept {
            assert(m_spans.size() > 1);
            return m_spans.top().m_begin >= m_spans[m_spans.size() - 2].m_end;
        }

        std::string_view view() const noexcept {
            return as_view().substr(m_spans.top().m_begin, m_spans.top().size());
        }
    };

}

#endif