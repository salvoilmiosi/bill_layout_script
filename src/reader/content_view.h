#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <string>

class content_view {
private:
    std::string m_text;
    size_t m_begin = 0;
    size_t m_end = std::string::npos;
    bool tokenized = false;

public:
    template<typename T>
    content_view(T &&text) : m_text(std::forward<T>(text)) {}

    void setbegin(size_t n) {
        m_begin += n;
    }

    void setend(size_t n) {
        if (n != std::string::npos) {
            m_end = m_begin + n;
        }
    }
    
    void next_token(const std::string &separator) {
        if (!tokenized) {
            m_end = m_begin;
            tokenized = true;
        }
        m_begin = m_text.find_first_not_of(separator, m_end);
        if (m_begin == std::string::npos) {
            m_end = std::string::npos;
        } else {
            m_end = m_text.find_first_of(separator, m_begin);
        }
    }

    bool token_end() {
        return m_begin >= m_text.size();
    }

    std::string_view view() const {
        return std::string_view(
            m_text.c_str() + std::min(m_begin, m_text.size()),
            m_text.c_str() + std::min(m_end, m_text.size())
        );
    }
};

#endif