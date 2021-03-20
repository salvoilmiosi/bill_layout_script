#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <variant>
#include <string>
#include <wx/datetime.h>
#include "fixed_point.h"

class variable {
public:
    variable() = default;

    variable(const std::string &value) : m_str(value) {}
    variable(std::string &&value) : m_str(std::move(value)) {}

    variable(std::string_view value) : m_data(value) {}

    variable(fixed_point value) : m_data(value) {}
    variable(std::floating_point auto value) : m_data(fixed_point(value)) {}

    variable(std::integral auto value) : m_data(int64_t(value)) {}

    variable(wxDateTime value) : m_data(value) {}

    bool is_string() const;
    bool is_number() const;

    const std::string &str() const & {
        return get_string();
    }

    std::string &&str() && {
        return std::move(get_string());
    }

    std::string_view str_view() const;
    fixed_point number() const;
    int64_t as_int() const;
    double as_double() const;
    wxDateTime date() const;

    bool as_bool() const;
    bool empty() const;

    std::partial_ordering operator <=> (const variable &other) const;
    
    bool operator == (const variable &other) const {
        return std::partial_ordering::equivalent == *this <=> other;
    }

    void assign(const variable &other);
    void assign(variable &&other);

    void append(const variable &other);

private:
    mutable std::string m_str;
    std::variant<std::monostate, std::string_view, fixed_point, int64_t, wxDateTime> m_data;

    std::string &get_string() const;
};

#endif