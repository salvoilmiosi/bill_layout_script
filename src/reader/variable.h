#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>

#include "decimal.h"

enum value_type {
    VALUE_UNDEFINED,
    VALUE_STRING,
    VALUE_NUMBER,
};

using fixed_point = dec::decimal<10>;

class variable {
public:
    variable() {}
    variable(const std::string &value, value_type type = VALUE_STRING) : m_value(value), m_type(type) {}
    variable(const std::string_view &value, value_type type = VALUE_STRING) : m_value(std::string(value)), m_type(type) {}
    variable(const fixed_point &value) : m_type(VALUE_NUMBER) {
        if (value.getAsDouble() == value.getAsInteger()) {
            m_value = std::to_string(value.getAsInteger());
        } else {
            m_value = dec::toString(value);
        }
    }

    template<typename T> variable(const T &value) : variable(fixed_point(value)) {}
    variable(const size_t &value) : variable(int(value)) {}

    value_type type() const {
        return m_type;
    }

    const std::string &str() const {
        return m_value;
    }

    fixed_point number() const {
        return fixed_point(m_value);
    }

    int asInt() const {
        return number().getAsInteger();
    }

    bool empty() const {
        return m_value.empty();
    }

    bool isTrue() const;

    bool operator == (const variable &other) const;
    bool operator != (const variable &other) const;
    bool operator < (const variable &other) const;
    bool operator > (const variable &other) const;

    variable operator + (const variable &other) const;
    variable operator - (const variable &other) const;
    variable operator * (const variable &other) const;
    variable operator / (const variable &other) const;

    bool debug = false;

private:
    std::string m_value;
    value_type m_type{VALUE_UNDEFINED};
};

#endif