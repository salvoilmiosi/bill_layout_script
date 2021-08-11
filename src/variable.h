#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <variant>
#include <string>
#include <vector>

#include "fixed_point.h"
#include "datetime.h"

namespace bls {

    typedef int64_t big_int;
    struct null_state {};
    struct string_state {};
    struct regex_state {};

    class variable {
    public:
        using variant_type = std::variant<null_state, string_state, regex_state, std::string_view, fixed_point, big_int, double, datetime, std::vector<variable>>;
        
        variable() = default;

        variable(const std::string &value) : m_str(value), m_value(string_state{}) {}
        variable(std::string &&value) : m_str(std::move(value)), m_value(string_state{}) {}

        variable(regex_state, const std::string &value) : m_str(value), m_value(regex_state{}) {}
        variable(regex_state, std::string &&value) : m_str(std::move(value)), m_value(regex_state{}) {}

        variable(std::string_view value) : m_value(value) {}

        variable(fixed_point value) : m_value(value) {}
        variable(std::integral auto value) : m_value(big_int(value)) {}
        variable(std::floating_point auto value) : m_value(double(value)) {}

        variable(datetime value) : m_value(value) {}

        template<typename T>
        variable(const std::vector<T> &vec) : m_value(std::vector<variable>(vec.begin(), vec.end())) {}

        bool is_null() const;
        bool is_string() const;
        bool is_regex() const;
        bool is_view() const;
        bool is_number() const;
        bool is_array() const;

        const std::string &as_string() const & {
            return get_string();
        }

        std::string &&as_string() && {
            return std::move(get_string());
        }

        std::string_view as_view() const;
        fixed_point as_number() const;
        big_int as_int() const;
        double as_double() const;
        datetime as_date() const;

        std::vector<variable> &as_array();
        const std::vector<variable> &as_array() const;

        bool as_bool() const;

        std::partial_ordering operator <=> (const variable &other) const;
        
        bool operator == (const variable &other) const {
            return std::partial_ordering::equivalent == *this <=> other;
        }

        void assign(const variable &other);
        void assign(variable &&other);

        variable &operator += (const variable &rhs);
        variable &operator -= (const variable &rhs);

        variable operator + (const variable &rhs) const;
        variable operator - () const;
        variable operator - (const variable &rhs) const;
        variable operator * (const variable &rhs) const;
        variable operator / (const variable &rhs) const;

    private:
        mutable std::string m_str;

        variant_type m_value;

        std::string &get_string() const;
    };

}

#endif