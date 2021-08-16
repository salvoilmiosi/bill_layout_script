#ifndef __VARIABLE_H__
#define __VARIABLE_H__

#include <variant>
#include <string>
#include <vector>
#include <memory>

#include "fixed_point.h"
#include "datetime.h"

namespace bls {

    typedef int64_t big_int;

    struct string_flags {
        bool is_regex;
    };

    static constexpr string_flags as_string_tag{false};
    static constexpr string_flags as_regex_tag{true};

    struct string_state : std::string_view {
        string_state() = default;

        string_state(std::string_view str, string_flags flags = as_string_tag)
            : std::string_view(str), flags(flags) {}
        
        string_flags flags;
    };

    class variable;

    using variable_array = std::vector<variable>;
    using variable_ptr = const variable *;

    class variable {
    public:
        using variant_type = std::variant<std::monostate, string_state, fixed_point, big_int, double, datetime, variable_array, variable_ptr>;
        
        variable() = default;

        variable(const variable &var);
        variable(variable &&var);

        ~variable() = default;

        variable(const std::string &value, string_flags state = as_string_tag)
            : m_str(std::make_unique<std::string>(value))
            , m_value(string_state(*m_str, state)) {}

        variable(std::string &&value, string_flags state = as_string_tag)
            : m_str(std::make_unique<std::string>(std::move(value)))
            , m_value(string_state(*m_str, state)) {}

        variable(std::string_view value, string_flags state = as_string_tag)
            : m_value(string_state(value, state)) {}

        variable(fixed_point value) : m_value(value) {}
        variable(std::integral auto value) : m_value(big_int(value)) {}
        variable(std::floating_point auto value) : m_value(double(value)) {}

        variable(datetime value) : m_value(value) {}

        variable(const variable_array &vec) : m_value(vec) {}
        variable(variable_array &&vec) : m_value(std::move(vec)) {}

        template<typename T>
        variable(const std::vector<T> &vec) : m_value(std::in_place_type<variable_array>, vec.begin(), vec.end()) {}

        template<typename T>
        variable(std::vector<T> &&vec) : m_value(std::in_place_type<variable_array>,
            std::make_move_iterator(vec.begin()), std::make_move_iterator(vec.end())
        ) {}

        template<std::ranges::input_range T> requires std::ranges::enable_view<T>
        variable(T range) : m_value(std::in_place_type<variable_array>, range.begin(), range.end()) {}

        variable(variable_ptr ptr) : m_value(ptr) {}

        bool is_null() const;
        bool is_pointer() const;
        bool is_string() const;
        bool is_regex() const;
        bool is_view() const;
        bool is_number() const;
        bool is_array() const;

        const std::string &as_string() const &;
        std::string as_string() &&;

        variable_array &as_array();
        const variable_array &as_array() const;

        const variable &deref() const &;
        variable deref() &&;

        string_state as_view() const;
        fixed_point as_number() const;
        big_int as_int() const;
        double as_double() const;
        datetime as_date() const;
        bool as_bool() const;
        variable_ptr as_pointer() const;

        std::partial_ordering operator <=> (const variable &other) const;
        
        bool operator == (const variable &other) const {
            return std::partial_ordering::equivalent == *this <=> other;
        }

        variable &operator = (const variable &other);
        variable &operator = (variable &&other);

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
        mutable std::unique_ptr<std::string> m_str;

        variant_type m_value;

        std::string &get_string() const;
    };

}

#endif