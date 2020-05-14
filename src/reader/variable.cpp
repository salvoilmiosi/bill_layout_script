#include "variable.h"

#include <iomanip>
#include <sstream>

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