#include "variable.h"
#include "reader.h"

#include "functions.h"

void variable::set_string(const std::string &str) {
    m_type = VAR_STRING;
    m_str = str;
    m_num = fixed_point(str);
}

void variable::set_number(const fixed_point &num) {
    m_type = VAR_NUMBER;
    m_num = num;
    m_str = dec::toString(num);
    auto it = m_str.rbegin();
    for (; *it == '0' && it != m_str.rend(); ++it);
    if (*it == '.') ++it;

    m_str.erase(it.base(), m_str.end());
}

variable variable::str_to_number(const std::string &str) {
    if (str.empty()) {
        return null_var();
    } else {
        return fixed_point(str);
    }
}

bool variable::operator == (const variable &other) const {
    if (m_type == other.m_type) {
        switch (m_type) {
        case VAR_STRING:
            return str() == other.str();
        case VAR_NUMBER:
            return number() == other.number();
        default:
            return true;
        }
    }
    return false;
}

bool variable::operator != (const variable &other) const {
    return !(*this == other);
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

bool variable::operator < (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return str() < other.str();
    default:
        return number() < other.number();
    }
}

bool variable::operator > (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return str() > other.str();
    default:
        return number() > other.number();
    }
}

bool variable::operator <= (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return str() <= other.str();
    default:
        return number() <= other.number();
    }
}

bool variable::operator >= (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return str() >= other.str();
    default:
        return number() >= other.number();
    }
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
    case VAR_STRING:
        set_string(str() + other.str());
        break;
    case VAR_NUMBER:
        set_number(number() + other.number());
        break;
    default:
        *this = other;
    }
    return *this;
}