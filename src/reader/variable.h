#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>
#include <variant>

#include "decimal.h"
#include "bytecode.h"

using fixed_point = dec::decimal<FLOAT_PRECISION>;

enum variable_type {
    VAR_UNDEFINED,
    VAR_STRING,
    VAR_NUMBER
};

class variable {
public:
    variable() {}

    variable(const std::string &value) : m_value(value), m_type(VAR_STRING) {}
    variable(std::string_view value) : m_value(std::string(value)), m_type(VAR_STRING) {}

    variable(const fixed_point &value) : m_value(value), m_type(VAR_NUMBER) {}
    template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    variable(T value) : m_value(fixed_point(value)), m_type(VAR_NUMBER) {}

    static const variable &null_var() {
        static const variable VAR_NULL;
        return VAR_NULL;
    }

    std::string str() const;
    std::string &strref();

    variable_type type() const {
        return m_type;
    }

    fixed_point number() const;
    int as_int() const;
    bool as_bool() const;

    operator std::string() const { return str(); }
    operator fixed_point() const { return number(); }
    operator int() const { return as_int(); }
    operator bool() const { return as_bool(); }

    bool empty() const;

    static variable str_to_number(const std::string &str);
    
    bool operator == (const variable &other) const;
    bool operator != (const variable &other) const;

    bool operator && (const variable &other) const;
    bool operator || (const variable &other) const;
    bool operator ! () const;
    
    bool operator < (const variable &other) const;
    bool operator > (const variable &other) const;
    bool operator <= (const variable &other) const;
    bool operator >= (const variable &other) const;

    variable operator - () const;
    variable operator + (const variable &other) const;
    variable operator - (const variable &other) const;
    variable operator * (const variable &other) const;
    variable operator / (const variable &other) const;

    variable &operator += (const variable &other);

private:
    std::variant<std::string, fixed_point> m_value;
    variable_type m_type = VAR_UNDEFINED;
};

#endif