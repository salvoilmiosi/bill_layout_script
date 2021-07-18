#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {
    class content_string {
    private:
        std::string m_str;
        std::string_view m_view;
    
    public:
        content_string(const std::string &str)
            : m_str(str)
            , m_view(m_str) {}
        
        content_string(std::string &&str) noexcept
            : m_str(std::move(str))
            , m_view(m_str) {}

        content_string(std::string_view str) noexcept
            : m_view(str) {}

        content_string(variable &&var) {
            if (var.is_view()) {
                *this = var.as_view();
            } else {
                *this = std::move(var).as_string();
            }
        }

        content_string(const content_string &other) {
            *this = other;
        }

        content_string(content_string &&other) noexcept {
            *this = std::move(other);
        }

        ~content_string() = default;
        
        content_string &operator = (const content_string &other) {
            if (other.m_str.empty()) {
                m_view = other.m_view;
            } else {
                size_t begin = other.m_view.data() - other.m_str.data();
                m_str = other.m_str;
                m_view = std::string_view(m_str.data() + begin, other.m_view.size());
            }
            return *this;
        }
        
        content_string &operator = (content_string &&other) noexcept {
            if (other.m_str.empty()) {
                m_view = other.m_view;
            } else {
                size_t begin = other.m_view.data() - other.m_str.data();
                m_str = std::move(other.m_str);
                m_view = std::string_view(m_str.data() + begin, other.m_view.size());
            }
            return *this;
        }

        void setbegin(size_t n) noexcept {
            m_view = m_view.substr(n);
        }

        void setend(size_t n) noexcept {
            m_view = m_view.substr(0, n);
        }

        variable view() const {
            return m_view;
        }
    };

    class content_list {
    private:
        size_t m_index{0};

        std::vector<variable> m_data;
    
    public:
        content_list(const std::vector<variable> &data) : m_data(data) {}
        content_list(std::vector<variable> &&data) : m_data(std::move(data)) {}

        void nextresult() {
            ++m_index;
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
        content_view(const std::string &str)
            : m_data(std::in_place_type<content_string>, str) {}
        content_view(std::string &&str)
            : m_data(std::in_place_type<content_string>, std::move(str)) {}

        content_view(variable &&var)
            : m_data(std::in_place_type<content_string>, std::move(var)) {}

        content_view(const std::vector<variable> &vec)
            : m_data(std::in_place_type<content_list>, vec) {}
        content_view(std::vector<variable> &&vec)
            : m_data(std::in_place_type<content_list>, std::move(vec)) {}

        template<typename T, typename ... Ts>
        content_view(std::in_place_type_t<T>, Ts && ... args)
            : m_data(std::in_place_type<T>, std::forward<Ts>(args) ...) {}

        void setbegin(size_t n) {
            std::get<content_string>(m_data).setbegin(n);
        }

        void setend(size_t n) {
            std::get<content_string>(m_data).setend(n);
        }

        void nextresult() {
            std::get<content_list>(m_data).nextresult();
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