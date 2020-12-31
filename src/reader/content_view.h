#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <string>

class content_view {
private:
    std::string m_text;
    size_t view_begin = 0;
    size_t view_end = std::string::npos;

public:
    template<typename T>
    content_view(T &&text) : m_text(std::forward<T>(text)) {}

    void setbegin(size_t n) {
        view_begin += n;
    }

    void setend(size_t n) {
        view_end = view_begin + n;
    }
    
    void next_token(const std::string &separator) {
        if (view_end == std::string::npos) {
            view_end = view_begin;
        }
        view_begin = m_text.find_first_not_of(separator, view_end);
        if (view_begin != std::string::npos) {
            view_end = std::string::npos;
        } else {
            view_end = m_text.find_first_of(separator, view_begin);
        }
    }

    bool token_end() {
        return view_begin == std::string::npos;
    }

    std::string_view view() const {
        return std::string_view(
            m_text.begin() + std::min(view_begin, m_text.size()),
            m_text.begin() + std::min(view_end, m_text.size())
        );
    }
};

#endif