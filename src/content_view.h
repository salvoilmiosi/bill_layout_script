#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <string>

struct view_span {
    size_t m_begin;
    size_t m_end;

    // Non necessario su c++20, ma clang da' errore se manca questa riga
    view_span(size_t m_begin, size_t m_end) : m_begin(m_begin), m_end(m_end) {}
    
    size_t size() const noexcept {
        return m_end - m_begin;
    }
};

class content_view {
private:
    std::string m_text;
    std::vector<view_span> m_spans;

public:
    template<typename T>
    content_view(T &&text) : m_text(std::forward<T>(text)) {
        m_spans.emplace_back(0, m_text.size());
    }

    void setbegin(size_t n) {
        m_spans.back().m_begin += n;
    }

    void setend(size_t n) {
        if (n < m_spans.back().size()) {
            m_spans.back().m_end = m_spans.back().m_begin + n;
        }
    }

    void new_view() {
        m_spans.push_back(m_spans.back());
    }

    void new_subview() {
        m_spans.emplace_back(m_spans.back().m_begin, m_spans.back().m_begin);
    }

    void reset_view() {
        if (m_spans.size() > 1) {
            m_spans.pop_back();
        }
    }
    
    void next_result() {
        if (m_spans.size() > 1) {
            m_spans.back().m_begin = m_text.find_first_not_of('\0', m_spans.back().m_end);
            if (token_end()) {
                m_spans.back().m_begin = m_spans.back().m_end = m_spans[m_spans.size() - 2].m_end;
            } else {
                m_spans.back().m_end = std::min(
                    m_text.find_first_of('\0', m_spans.back().m_begin),
                    m_spans[m_spans.size() - 2].m_end);
            }
        }
    }

    bool token_end() {
        return m_spans.size() > 1 && m_spans.back().m_begin >= m_spans[m_spans.size() - 2].m_end;
    }

    std::string_view view() const {
        return std::string_view(
            m_text.data() + m_spans.back().m_begin,
            m_spans.back().size());
    }
};

#endif