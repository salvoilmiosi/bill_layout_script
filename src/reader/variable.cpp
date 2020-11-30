#include "variable.h"
#include "reader.h"

#include "functions.h"

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

variable variable::parse_number() {
    if (m_type == VAR_STRING) {
        std::string out;
        for (char c : std::get<std::string>(m_value)) {
            if (std::isdigit(c) || c == '-') {
                out += c;
            } else if (c == DECIMAL_POINT) {
                out += '.';
            }
        }
        if (out.empty()) {
            return null_var();
        } else {
            return fixed_point(out);
        }
    } else {
        return *this;
    }
}

bool variable::operator == (const variable &other) const {
    return m_type == other.m_type && (m_type == VAR_UNDEFINED || m_value == other.m_value);
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
    m_value = number() + other.number();
    m_type = VAR_NUMBER;
    return *this;
}