#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <string>
#include <string_view>

#include "fixed_point.h"

enum variable_type : uint8_t {
    VAR_UNDEFINED,
    VAR_STRING,
    VAR_NUMBER,
    VAR_DATE,
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
    variable() noexcept = default;
    ~variable() noexcept = default;

    variable(const variable &) noexcept = default;
    variable(variable &&) noexcept = default;

    variable &operator = (const variable &other) noexcept;
    variable &operator = (variable &&other) noexcept;

    variable(const std::string &value) noexcept : m_type(VAR_STRING), m_str(value) {}
    variable(std::string &&value) noexcept : m_type(VAR_STRING), m_str(std::move(value)) {}

    variable(std::string_view value) noexcept : m_type(VAR_STRING), m_view(value) {}

    variable(fixed_point value) noexcept : m_type(VAR_NUMBER), m_num(value) {}

    template<typename T> requires(std::is_arithmetic_v<T>)
    variable(T value) noexcept : m_type(VAR_NUMBER), m_num(fixed_point(typename signed_type<T>::type(value))) {}

    variable(time_t value) noexcept : m_type(VAR_DATE) {
        m_num.setUnbiased(value);
    }

    static const variable &null_var() noexcept {
        static const variable VAR_NULL;
        return VAR_NULL;
    }
    
    variable_type type() const noexcept { return m_type; }

    const std::string &str() const & noexcept {
        set_string();
        return m_str;
    }

    std::string &&str() && noexcept {
        set_string();
        return std::move(m_str);
    }

    std::string_view str_view() const noexcept {
        if (m_view.empty()) {
            set_string();
            return m_str;
        }
        return m_view;
    }

    const fixed_point &number() const noexcept {
        set_number();
        return m_num;
    }

    time_t date() const noexcept {
        set_date();
        return m_num.getUnbiased();
    }

    int as_int() const noexcept {
        return number().getAsInteger();
    }

    double as_double() const noexcept {
        return number().getAsDouble();
    }

    bool as_bool() const noexcept;

    bool empty() const noexcept;

    std::partial_ordering operator <=> (const variable &other) const noexcept;
    
    bool operator == (const variable &other) const noexcept {
        return std::partial_ordering::equivalent == *this <=> other;
    }

    variable &operator += (const variable &other) noexcept;

private:
    variable_type m_type = VAR_UNDEFINED;

    mutable std::string m_str;
    mutable std::string_view m_view;
    mutable fixed_point m_num;

    void set_string() const noexcept;
    void set_number() const noexcept;
    void set_date() const noexcept;
};

#endif