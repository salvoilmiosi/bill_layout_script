#ifndef __CONTENT_VIEW_H__
#define __CONTENT_VIEW_H__

#include <span>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {

    class content_view {
    private:
        variable m_data;

        std::span<const variable> m_span;
        std::span<const variable>::iterator m_current;

    public:
        content_view(const std::string &str)
            : m_data(str)
            , m_span(&m_data, 1)
            , m_current(m_span.begin()) {}

        content_view(std::string &&str)
            : m_data(std::move(str))
            , m_span(&m_data, 1)
            , m_current(m_span.begin()) {}

        content_view(const variable &var, bool as_array) {
            if (as_array && var.is_array()) {
                m_data = var;
                m_span = m_data.deref().as_array();
            } else {
                if (var.is_view() || var.is_pointer()) {
                    m_data = var.as_view();
                } else {
                    m_data = var.as_string();
                }
                m_span = std::span(&m_data, 1);
            }
            m_current = m_span.begin();
        }

        content_view(variable &&var, bool as_array) {
            if (as_array && var.is_array()) {
                if (var.is_pointer()) {
                    m_data = var;
                    m_span = m_data.deref().as_array();
                } else {
                    m_data = std::move(var);
                    m_span = m_data.as_array();
                }
            } else {
                if (var.is_view() || var.is_pointer()) {
                    m_data = var.as_view();
                } else {
                    m_data = std::move(var).as_string();
                }
                m_span = std::span(&m_data, 1);
            }
            m_current = m_span.begin();
        }

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