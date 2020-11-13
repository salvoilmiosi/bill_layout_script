#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>
#include <variant>

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
    variable(const std::string &value) : m_value(value), m_type(VALUE_STRING) {}
    variable(std::string_view value) : m_value(std::string(value)), m_type(VALUE_STRING) {}

    variable(const fixed_point &value) : m_value(value), m_type(VALUE_NUMBER) {}
    template<typename T> variable(T value) : m_value(fixed_point(value)), m_type(VALUE_NUMBER) {}

    value_type type() const {
        return m_type;
    }

    std::string str() const;

    fixed_point number() const;

    int asInt() const;

    bool empty() const;

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
    std::variant<std::string, fixed_point> m_value;
    value_type m_type{VALUE_UNDEFINED};
};

#endif