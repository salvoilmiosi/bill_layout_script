#include "variable.h"
#include "reader.h"

#include "utils.h"

#include <regex>
#include <wx/datetime.h>

variable &variable::operator = (const variable &other) noexcept {
    m_type = other.m_type;
    if (other.m_view.empty()) {
        m_str = other.m_str;
    } else {
        m_str = other.m_view;
    }
    m_view = std::string_view();
    m_num = other.m_num;
    return *this;
}

variable &variable::operator = (variable &&other) noexcept {
    m_type = other.m_type;
    if (other.m_view.empty()) {
        m_str = std::move(other.m_str);
    } else {
        m_str = other.m_view;
    }
    m_view = std::string_view();
    m_num = other.m_num;
    return *this;
}

void variable::set_string() const noexcept {
    if (m_str.empty()) {
        switch (m_type) {
        case VAR_NUMBER:
            m_str = fixed_point_to_string(m_num);
            break;
        case VAR_STRING:
            if (!m_view.empty()) {
                m_str = m_view;
            } 
            break;
        case VAR_DATE:
            m_str = wxDateTime(time_t(m_num.getUnbiased())).FormatISODate().ToStdString();
            break;
        default:
            break;
        }
    }
}

void variable::set_number() const noexcept {
    if (m_type == VAR_STRING && m_num == fixed_point(0)) {
        if (m_str.empty() && !m_view.empty()) {
            m_str = m_view;
        }
        m_num = fixed_point(m_str);
    }
}

void variable::set_date() const noexcept {
    if (m_type == VAR_STRING && m_num == fixed_point(0)) {
        wxDateTime dt;
        if (dt.ParseISODate(str())) {
            m_num.setUnbiased(dt.GetTicks());
        }
    }
}

bool variable::as_bool() const noexcept {
    switch (m_type) {
    case VAR_STRING:
        return !str_view().empty();
    case VAR_NUMBER:
    case VAR_DATE:
        return m_num != fixed_point(0);
    default:
        return false;
    }
}

bool variable::empty() const noexcept {
    switch (m_type) {
    case VAR_STRING:
        return str_view().empty();
    case VAR_NUMBER:
        return false;
    case VAR_DATE:
        return m_num == fixed_point(0);
    default:
        return true;
    }
}

std::partial_ordering variable::operator <=> (const variable &other) const noexcept {
    switch (m_type) {
    case VAR_STRING:
        switch (other.m_type) {
        case VAR_STRING:
            return str_view() <=> other.str_view();
        case VAR_NUMBER:
        case VAR_DATE:
            return std::partial_ordering::unordered;
        default:
            return str_view() <=> "";
        }
    case VAR_NUMBER:
        switch (other.m_type) {
        case VAR_NUMBER:
            return number().getUnbiased() <=> other.number().getUnbiased();
        case VAR_STRING:
        case VAR_DATE:
            return std::partial_ordering::unordered;
        default:
            return number().getUnbiased() <=> 0;
        }
    case VAR_DATE:
        switch (other.m_type) {
        case VAR_DATE:
            return number().getUnbiased() <=> other.number().getUnbiased();
        default:
            return std::partial_ordering::unordered;
        }
    default:
        switch (other.m_type) {
        case VAR_STRING:
            return "" <=> other.str_view();
        case VAR_NUMBER:
            return 0 <=> other.number().getUnbiased();
        case VAR_DATE:
            return std::partial_ordering::unordered;
        default:
            return std::partial_ordering::equivalent;
        }
    }
}

variable &variable::operator += (const variable &other) noexcept {
    switch (other.m_type) {
    case VAR_NUMBER:
        set_number();
        m_type = VAR_NUMBER;
        m_num += other.number();
        m_str.clear();
        break;
    case VAR_STRING: {
        set_string();
        m_type = VAR_STRING;
        auto view = other.str_view();
        m_str.append(view.begin(), view.end());
        m_num = fixed_point(0);
        break;
    }
    default:
        break;
    }
    m_view = std::string_view();
    return *this;
}