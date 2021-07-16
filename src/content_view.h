#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {
    class content_string {
    private:
        size_t m_begin{0};
        size_t m_end{0};
    
        std::variant<std::string, std::string_view> m_data;

        void init(std::string_view str) {
            m_end = str.size();
            m_data = str;
        }

        void init(std::string &&str) {
            m_end = str.size();
            m_data = std::move(str);
        }
    
    public:
        content_string(variable &&var) {
            if (var.is_view()) {
                init(var.as_view());
            } else {
                init(std::move(var).as_string());
            }
        }

        content_string(std::string &&str) {
            init(std::move(str));
        }

        content_string(std::string_view str) {
            init(str);
        }

        void setbegin(size_t n) noexcept {
            m_begin = std::min(m_begin + n, m_end);
        }

        void setend(size_t n) noexcept {
            m_end = std::min(m_begin + n, m_end);
        }

        variable view() const {
            const char *data = std::visit(util::overloaded{
                [](const std::string &str) { return str.data(); },
                [](std::string_view str) { return str.data(); }
            }, m_data);
            return std::string_view(data + m_begin, data + m_end);
        }
    };

    class content_list {
    private:
        size_t m_index{0};

        std::vector<variable> m_data;
    
    public:
        content_list(variable &&var) : m_data(std::move(var).as_array()) {}

        void nextresult() {
            ++m_index;
        }

        size_t tokenidx() const {
            return m_index;
        }

        bool tokenend() const {
            return m_index >= m_data.size();
        }

        variable view() const {
            if (tokenend()) return {};
            return m_data[m_index];
        }
    };

    class content_view {
    private:
        std::variant<content_string, content_list> m_data;

    public:
        content_view(content_string &&str) : m_data(std::move(str)) {}
        content_view(content_list &&list) : m_data(std::move(list)) {}

        void setbegin(size_t n) {
            std::get<content_string>(m_data).setbegin(n);
        }

        void setend(size_t n) {
            std::get<content_string>(m_data).setend(n);
        }

        void nextresult() {
            std::get<content_list>(m_data).nextresult();
        }

        size_t tokenidx() const {
            return std::get<content_list>(m_data).tokenidx();
        }

        bool tokenend() const {
            return std::get<content_list>(m_data).tokenend();
        }

        variable view() const {
            return std::visit(util::overloaded {
                [](const content_string &str) { return str.view(); },
                [](const content_list &list) { return list.view(); }
            }, m_data);
        }
    };

}

#endif