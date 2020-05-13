#include "variable.h"

#include <iomanip>
#include <sstream>

variable& variable::operator = (const variable &other) {
    if (other.m_type != m_type && m_type != VALUE_UNDEFINED) {
        throw parsing_error(std::string("Can't set a ") + TYPES[other.m_type] + " to a " + TYPES[m_type], "");
    }
    m_type = other.m_type;
    if (other.m_type == VALUE_NUMBER && m_type != VALUE_UNDEFINED) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(std::min(precision(), other.precision())) << other.asNumber();
        m_value = stream.str();
    } else {
        m_value = other.m_value;
    }
    return *this;
}

bool variable::operator == (const variable &other) const {
    switch(other.m_type) {
    case VALUE_STRING:
        return m_value == other.m_value;
    case VALUE_NUMBER:
        return asNumber() == other.asNumber();
    default:
        return false;
    }
}

bool variable::operator != (const variable &other) const {
    return !(*this == other);
}

int variable::precision() const {
    if (m_type == VALUE_NUMBER) {
        int dot_pos = m_value.find_last_of('.');
        if (dot_pos >= 0) {
            return m_value.size() - dot_pos - 1;
        } else {
            return 0;
        }
    }
    return 0;
}

const std::string &variable::asString() const {
    return m_value;
}

float variable::asNumber() const {
    switch(m_type) {
    case VALUE_STRING:
        return m_value.empty() ? 1 : 0;
    case VALUE_NUMBER:
        try {
            return std::stof(m_value);
        } catch (std::invalid_argument &) {
            return 0;
        }
    default:
        return 0;
    }
}