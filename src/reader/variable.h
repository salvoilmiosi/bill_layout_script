#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>

#include "decimal.h"
#include "bytecode.h"

using fixed_point = dec::decimal<FLOAT_PRECISION>;

enum variable_type {
    VAR_UNDEFINED,
    VAR_STRING,
    VAR_NUMBER
};

class variable {
private:
    template<typename T> struct signed_type {
        using type = T;
    };
    template<typename T> requires (std::is_unsigned_v<T> && !std::is_same_v<bool, T>)
    struct signed_type<T> {
        using type = std::make_signed_t<T>;
    };

public:
    variable() {}

    variable(const std::string &value) { set_string(value); }
    variable(std::string_view value) { set_string(std::string(value)); }

    variable(const fixed_point &value) { set_number(value); }

    template<typename T> requires(std::is_arithmetic_v<T>)
    variable(T value) { set_number(fixed_point(typename signed_type<T>::type(value))); }

    static const variable &null_var() {
        static const variable VAR_NULL;
        return VAR_NULL;
    }

    static variable str_to_number(const std::string &str);
    
    variable_type type() const { return m_type; }

    std::string str() const { return m_str; }
    std::string &str() { return m_str; }

    fixed_point number() const { return m_num; }
    fixed_point &number() { return m_num; }

    int as_int() const { return m_num.getAsInteger(); }
    bool as_bool() const;

    bool empty() const { return m_str.empty(); }
    
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
    std::string m_str;
    fixed_point m_num;
    variable_type m_type = VAR_UNDEFINED;

    void set_string(const std::string &str);
    void set_number(const fixed_point &num);
};

#endif