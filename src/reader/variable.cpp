#include "variable.h"
#include "reader.h"

bool variable::isTrue() const {
    switch(m_type) {
    case VALUE_STRING:
        return !m_value.empty();
    case VALUE_NUMBER:
        return number() != fixed_point(0);
    default:
        return false;
    }
}

bool variable::operator == (const variable &other) const {
    switch(other.m_type) {
    case VALUE_STRING:
        return m_value == other.m_value;
    case VALUE_NUMBER:
        return number() == other.number();
    default:
        return false;
    }
}

bool variable::operator != (const variable &other) const {
    return !(*this == other);
}

bool variable::operator < (const variable &other) const {
    return number() < other.number();
}

bool variable::operator > (const variable &other) const {
    return number() > other.number();
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