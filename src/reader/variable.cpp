#include "variable.h"
#include "reader.h"

#include "utils.h"

std::string variable::str() const {
    switch (m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value);
    case VAR_NUMBER:
    {
        std::string s = dec::toString(std::get<fixed_point>(m_value));
        auto it = s.rbegin();
        for (; *it == '0' && it != s.rend(); ++it);
        if (*it == '.') ++it;

        s.resize(s.size() - (it - s.rbegin()));
        return s;
    }
    default:
        return "";
    }
}

std::string &variable::strref() {
    return std::get<std::string>(m_value);
}

fixed_point variable::number() const {
    switch (m_type) {
    case VAR_STRING:
        return fixed_point(std::get<std::string>(m_value));
    case VAR_NUMBER:
        return std::get<fixed_point>(m_value);
    default:
        return fixed_point(0);
    }
}

int variable::as_int() const {
    switch (m_type) {
    case VAR_STRING:
        try {
            return std::stoi(std::get<std::string>(m_value));
        } catch (const std::invalid_argument &) {
            return 0;
        }
    case VAR_NUMBER:
        return std::get<fixed_point>(m_value).getAsInteger();
    default:
        return 0;
    }
}

bool variable::as_bool() const {
    switch(m_type) {
    case VAR_STRING:
        return ! std::get<std::string>(m_value).empty();
    case VAR_NUMBER:
        return std::get<fixed_point>(m_value) != fixed_point(0);
    default:
        return false;
    }
}

bool variable::empty() const {
    switch (m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value).empty();
    case VAR_NUMBER:
        return false;
    default:
        return true;
    }
}

fixed_point variable::str_to_number() {
    switch (m_type) {
    case VAR_STRING:
        return fixed_point(parse_number(std::get<std::string>(m_value)));
    case VAR_NUMBER:
        return std::get<fixed_point>(m_value);
    default:
        return fixed_point(0);
    }
}

bool variable::operator == (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value) == other.str();
    case VAR_NUMBER:
        return std::get<fixed_point>(m_value) == other.number();
    default:
        return !other.as_bool();
    }
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
        return std::get<std::string>(m_value) < other.str();
    default:
        return number() < other.number();
    }
}

bool variable::operator > (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value) > other.str();
    default:
        return number() > other.number();
    }
}

bool variable::operator <= (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value) <= other.str();
    default:
        return number() <= other.number();
    }
}

bool variable::operator >= (const variable &other) const {
    switch(m_type) {
    case VAR_STRING:
        return std::get<std::string>(m_value) >= other.str();
    default:
        return number() >= other.number();
    }
}

variable variable::operator - () const {
    if (empty()) {
        return *this;
    } else {
        return - number();
    }
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