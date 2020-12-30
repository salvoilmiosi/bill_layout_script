#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>
#include <string_view>

#include "decimal.h"
#include "bytecode.h"

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
    variable() = default;
    ~variable() = default;

    variable(const variable &) = default;
    variable(variable &&) = default;

    variable &operator = (const variable &other);
    variable &operator = (variable &&other);

    variable(const std::string &value) : m_type(VAR_STRING), m_str(value) {};
    variable(std::string &&value) : m_type(VAR_STRING), m_str(std::move(value)) {};

    variable(std::string_view value) : m_type(VAR_STRING), m_view(value) {};

    variable(fixed_point value) : m_type(VAR_NUMBER), m_num(value) {}

    template<typename T> requires(std::is_arithmetic_v<T>)
    variable(T value) : m_type(VAR_NUMBER), m_num(fixed_point(typename signed_type<T>::type(value))) {}

    static const variable &null_var() {
        static const variable VAR_NULL;
        return VAR_NULL;
    }

    static variable str_to_number(const std::string &str);
    
    variable_type type() const { return m_type; }

    std::string &str() const {
        set_string();
        return m_str;
    }

    std::string_view str_view() const {
        if (m_view.empty()) {
            set_string();
            return m_str;
        }
        return m_view;
    }

    const fixed_point &number() const {
        set_number();
        return m_num;
    }

    int as_int() const {
        return number().getAsInteger();
    }

    double as_double() const {
        return number().getAsDouble();
    }

    bool as_bool() const;

    bool empty() const;

    std::partial_ordering operator <=> (const variable &other) const;
    bool operator == (const variable &other) const {
        return std::partial_ordering::equivalent == *this <=> other;
    }

    bool operator && (const variable &other) const;
    bool operator || (const variable &other) const;
    bool operator ! () const;

    variable operator - () const;
    variable operator + (const variable &other) const;
    variable operator - (const variable &other) const;
    variable operator * (const variable &other) const;
    variable operator / (const variable &other) const;

    variable &operator += (const variable &other);

private:
    variable_type m_type = VAR_UNDEFINED;

    mutable std::string m_str;
    mutable std::string_view m_view;
    mutable fixed_point m_num;

    void set_string() const;
    void set_number() const;
};

#endif