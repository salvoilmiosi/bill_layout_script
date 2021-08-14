#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <cassert>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {

    class content_view {
    private:
        size_t m_index = 0;
        variable m_data;

        void init(const variable &var, bool as_array) {
            if (as_array && var.is_array()) {
                m_data = var;
            } else if (var.is_view() || var.is_pointer()) {
                m_data = std::vector{var.as_view()};
            } else {
                m_data = std::vector{var.as_string()};
            }
        }

        void init(variable &&var, bool as_array) {
            if (as_array && var.is_array()) {
                if (var.is_pointer()) {
                    m_data = var;
                } else {
                    m_data = std::move(var);
                }
            } else if (var.is_view() || var.is_pointer()) {
                m_data = std::vector{var.as_view()};
            } else {
                m_data = std::vector{std::move(var).as_string()};
            }
        }

    public:
        content_view(const std::string &str) : m_data(std::vector{str}) {}
        content_view(std::string &&str) : m_data(std::vector{std::move(str)}) {}

        content_view(const variable &var, bool as_array) { init(var, as_array); }
        content_view(variable &&var, bool as_array) { init(std::move(var), as_array); }

        size_t index() const {
            return m_index;
        }

        void nextresult() {
            ++m_index;
        }

        bool tokenend() const {
            return m_index >= m_data.as_array().size();
        }

        variable view() const {
            if (tokenend()) return {};
            const auto &var = m_data.as_array()[m_index];
            if (var.is_string()) {
                return var.as_view();
            } else {
                return var.as_pointer();
            }
        }
    };

}

#endif