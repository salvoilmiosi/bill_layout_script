#ifndef __VARIABLE_VIEW_H__
#define __VARIABLE_VIEW_H__

#include <span>

#include "variable.h"
#include "utils.h"
#include "stack.h"

namespace bls {

    struct as_array_tag_t {};
    constexpr as_array_tag_t as_array_tag;

    class variable_view {
    private:
        std::span<const variable> m_span;
        std::span<const variable>::iterator m_current;

    public:
        variable_view(const variable &var) {
            m_span = std::span(var.as_pointer(), 1);
            m_current = m_span.begin();
        }

        variable_view(variable &&var) = delete;

        variable_view(const variable &var, as_array_tag_t) {
            if (var.is_array()) {
                m_span = var.as_array();
                m_current = m_span.begin();
            } else if (!var.is_null()) {
                throw conversion_error(intl::translate("CANT_CONVERT_TYPE_TO_TYPE", intl::enum_label(var.type()), intl::enum_label(variable_type::ARRAY)));
            }
        }
        
        variable_view(variable &&var, as_array_tag_t) = delete;

        size_t index() const {
            return m_current - m_span.begin();
        }

        void nextview() {
            ++m_current;
        }

        bool ate() const {
            return m_current >= m_span.end();
        }

        variable view() const {
            if (ate()) return {};
            return m_current->as_pointer();
        }
    };

}

#endif