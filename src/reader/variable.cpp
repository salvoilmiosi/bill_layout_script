#include "variable.h"
#include "reader.h"

#include "functions.h"

variable &variable::operator = (const variable &other) {
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

variable &variable::operator = (variable &&other) {
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

void variable::set_string() const {
    if (m_str.empty()) {
        switch (m_type) {
        case VAR_NUMBER:
        {
            m_str = dec::toString(m_num);
            auto it = m_str.rbegin();
            for (; *it == '0' && it != m_str.rend(); ++it);
            if (*it == '.') ++it;

            m_str.erase(it.base(), m_str.end());
            break;
        }
        case VAR_STRING:
            if (!m_view.empty()) {
                m_str = m_view;
            } 
            break;
        default:
            break;
        }
    }
}

void variable::set_number() const {
    if (m_type == VAR_STRING && m_num == fixed_point(0)) {
        if (m_str.empty() && !m_view.empty()) {
            m_str = m_view;
        }
        m_num = fixed_point(m_str);
    }
}

variable variable::str_to_number(const std::string &str) {
    if (str.empty()) {
        return null_var();
    } else {
        return fixed_point(str);
    }
}

bool variable::as_bool() const {
    switch (m_type) {
    case VAR_STRING:
        return !str_view().empty();
    case VAR_NUMBER:
        return m_num != fixed_point(0);
    default:
        return false;
    }
}

bool variable::empty() const {
    switch (m_type) {
    case VAR_STRING:
        return str_view().empty();
    case VAR_NUMBER:
        return false;
    default:
        return true;
    }
}

std::partial_ordering variable::operator <=> (const variable &other) const {
    if (m_type == other.m_type) {
        switch (m_type) {
        case VAR_STRING:
            return str_view() <=> other.str_view();
        case VAR_NUMBER:
            return number().getUnbiased() <=> other.number().getUnbiased();
        default:
            return std::partial_ordering::equivalent;
        }
    }
    return std::partial_ordering::unordered;
}

bool variable::operator && (const variable &other) const {
    return as_bool() && other.as_bool();
}

bool variable::operator || (const variable &other) const {
    return as_bool() || other.as_bool();
}

bool variable::operator ! () const {
    return ! as_bool();
}

variable variable::operator - () const {
    return - number();
}

variable variable::operator + (const variable &other) const {
    return number() + other.number();
}

variable variable::operator - (const variable &other) const {
    return number() - other.number();
}

variable variable::operator * (const variable &other) const {
    return number() * other.number();
}

variable variable::operator / (const variable &other) const {
    return number() / other.number();
}

variable &variable::operator += (const variable &other) {
    switch (other.m_type) {
    case VAR_NUMBER:
        return *this = number() + other.number();
    case VAR_STRING:
        return *this = str() + other.str();
    default:
        return *this;
    }
}