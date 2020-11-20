#include "variable.h"
#include "reader.h"

#include "utils.h"

std::string variable::str() const {
    switch (m_value.index()) {
    case 0:
        return std::get<std::string>(m_value);
    case 1:
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

fixed_point variable::number() const {
    switch (m_value.index()) {
    case 0:
        return fixed_point(std::get<std::string>(m_value));
    case 1:
        return std::get<fixed_point>(m_value);
    default:
        return fixed_point(0);
    }
}

int variable::as_int() const {
    switch (m_value.index()) {
    case 0:
        try {
            return std::stoi(std::get<std::string>(m_value));
        } catch (const std::invalid_argument &) {
            return 0;
        }
    case 1:
        return std::get<fixed_point>(m_value).getAsInteger();
    default:
        return 0;
    }
}

bool variable::as_bool() const {
    switch(m_value.index()) {
    case 0:
        return ! std::get<std::string>(m_value).empty();
    case 1:
        return std::get<fixed_point>(m_value) != fixed_point(0);
    default:
        return false;
    }
}

bool variable::empty() const {
    switch (m_value.index()) {
    case 0:
        return std::get<std::string>(m_value).empty();
    case 1:
        return false;
    default:
        return true;
    }
}

fixed_point variable::str_to_number() {
    switch (m_value.index()) {
    case 0:
        return fixed_point(parse_number(std::get<std::string>(m_value)));
    case 1:
        return std::get<fixed_point>(m_value);
    default:
        return fixed_point(0);
    }
}

bool variable::operator == (const variable &other) const {
    switch(other.m_value.index()) {
    case 0:
        return std::get<std::string>(m_value) == other.str();
    case 1:
        return std::get<fixed_point>(m_value) == other.number();
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

bool variable::operator <= (const variable &other) const {
    return number() <= other.number();
}

bool variable::operator >= (const variable &other) const {
    return number() >= other.number();
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