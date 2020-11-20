#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>
#include <variant>

#include "decimal.h"
#include "bytecode.h"

using fixed_point = dec::decimal<FLOAT_PRECISION>;

class variable {
public:
    variable() {}

    variable(const std::string &value) : m_value(value) {}
    variable(std::string_view value) : m_value(std::string(value)) {}

    variable(const fixed_point &value) : m_value(value) {}
    template<typename T> variable(T value) : m_value(fixed_point(value)) {}

    static const variable &null_var() {
        static const variable VAR_NULL;
        return VAR_NULL;
    }

    std::string str() const;
    fixed_point number() const;
    int as_int() const;
    bool as_bool() const;

    bool empty() const;

    fixed_point str_to_number();

    bool operator == (const variable &other) const;
    bool operator != (const variable &other) const;
    bool operator < (const variable &other) const;
    bool operator > (const variable &other) const;
    bool operator <= (const variable &other) const;
    bool operator >= (const variable &other) const;

    variable operator - () const;
    variable operator + (const variable &other) const;
    variable operator - (const variable &other) const;
    variable operator * (const variable &other) const;
    variable operator / (const variable &other) const;

private:
    std::variant<std::string, fixed_point> m_value;
};

#endif