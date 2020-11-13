#include "variable.h"
#include "reader.h"

std::string variable::str() const {
    switch (m_type) {
    case VALUE_STRING:
        return std::get<std::string>(m_value);
    case VALUE_NUMBER:
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
    switch (m_type) {
    case VALUE_STRING:
        return dec::fromString<fixed_point>(std::get<std::string>(m_value));
    case VALUE_NUMBER:
        return std::get<fixed_point>(m_value);
    default:
        return fixed_point(0);
    }
}

int variable::asInt() const {
    switch (m_type) {
    case VALUE_STRING:
        try {
            return std::stoi(std::get<std::string>(m_value));
        } catch (std::invalid_argument &) {
            return 0;
        }
    case VALUE_NUMBER:
        return std::get<fixed_point>(m_value).getAsInteger();
    default:
        return 0;
    }
}

bool variable::empty() const {
    switch (m_type) {
    case VALUE_STRING:
        return std::get<std::string>(m_value).empty();
    case VALUE_NUMBER:
        return false;
    default:
        return true;
    }
}

bool variable::isTrue() const {
    switch(m_type) {
    case VALUE_STRING:
        return ! std::get<std::string>(m_value).empty();
    case VALUE_NUMBER:
        return std::get<fixed_point>(m_value) != fixed_point(0);
    default:
        return false;
    }
}

bool variable::operator == (const variable &other) const {
    switch(other.m_type) {
    case VALUE_STRING:
        return std::get<std::string>(m_value) == other.str();
    case VALUE_NUMBER:
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