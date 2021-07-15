#include "variable.h"

#include "utils.h"
#include "exceptions.h"

using namespace bls;

template<typename T, typename ... Ts> struct is_one_of : std::false_type {};
template<typename T, typename First, typename ... Ts>
struct is_one_of<T, First, Ts...> : std::bool_constant<is_one_of<T, Ts...>::value> {};
template<typename T, typename ... Ts> struct is_one_of<T, T, Ts...> : std::true_type {};
template<typename T, typename ... Ts> constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

template<typename T> concept string_t = is_one_of_v<T, string_state, std::string_view>;
template<typename T> concept number_t = is_one_of_v<T, fixed_point, big_int, double>;
template<typename T> concept not_number_t = ! number_t<T>;

std::string &variable::get_string() const {
    if (m_str.empty()) {
        m_str = std::visit(util::overloaded{
            [](auto)                    { return std::string(); },
            [](std::string_view str)    { return std::string(str); },
            [](fixed_point num)         { return fixed_point_to_string(num); },
            [](big_int num)             { return util::to_string(num); },
            [](double num)              { return util::to_string(num); },
            [](datetime date)           { return date.to_string(); },
            [](const std::vector<variable> &arr) {
                return util::string_join(arr | std::views::transform(
                [](const variable &var) { return var.as_view(); }), util::unit_separator);
            }
        }, m_value);
    }
    return m_str;
}

std::string_view variable::as_view() const {
    return std::visit(util::overloaded{
        [&](auto)                   { return std::string_view(as_string()); },
        [](std::string_view str)    { return str; },
        [](null_state)              { return std::string_view(); }
    }, m_value);
}

fixed_point variable::as_number() const {
    return std::visit(util::overloaded{
        [&](string_t auto)      { return fixed_point(as_string()); },
        [](number_t auto num)   { return fixed_point(num); },
        [](auto)                { return fixed_point(0); }
    }, m_value);
}

big_int variable::as_int() const {
    return std::visit(util::overloaded{
        [&](string_t auto) {
            big_int num = 0;
            auto view = as_view();
            std::from_chars(view.data(), view.data() + view.size(), num);
            return num;
        },
        [](fixed_point num) { return num.getAsInteger(); },
        [](big_int num)     { return num; },
        [](double num)      { return big_int(num); },
        [](auto)            { return big_int(0); }
    }, m_value);
}

double variable::as_double() const {
    return std::visit(util::overloaded{
        [&](string_t auto)  { return as_number().getAsDouble(); },
        [](fixed_point num) { return num.getAsDouble(); },
        [](big_int num)     { return double(num); },
        [](double num)      { return num; },
        [](auto)            { return 0.0; }
    }, m_value);
}

datetime variable::as_date() const {
    return std::visit(util::overloaded{
        [&](string_t auto) { return datetime::from_string(as_string()); },
        [](datetime date) { return date; },
        [](auto)            { return datetime(); }
    }, m_value);
}

const std::vector<variable> &variable::as_array() const {
    if (const std::vector<variable> *val = std::get_if<std::vector<variable>>(&m_value)) {
        return *val;
    } else {
        throw layout_error("variabile non array");
    }
}

std::vector<variable> &variable::as_array() {
    if (std::vector<variable> *val = std::get_if<std::vector<variable>>(&m_value)) {
        return *val;
    } else {
        throw layout_error("variabile non array");
    }
}

bool variable::as_bool() const {
    return std::visit(util::overloaded{
        [](null_state)              { return false; },
        [&](string_state)           { return !m_str.empty(); },
        [](std::string_view str)    { return !str.empty(); },
        [](number_t auto num)       { return num != 0; },
        [](datetime date)           { return date.is_valid(); },
        [](const std::vector<variable> &arr) { return !arr.empty(); }
    }, m_value);
}

bool variable::is_null() const {
    return std::visit(util::overloaded{
        [](null_state)              { return true; },
        [](auto)                    { return false; },
        [&](string_state)           { return m_str.empty(); },
        [](std::string_view str)    { return str.empty(); },
        [](datetime date)           { return !date.is_valid(); },
        [](const std::vector<variable> &arr) { return arr.empty(); }
    }, m_value);
}

bool variable::is_string() const {
    return std::visit(util::overloaded{
        [](string_t auto)   { return true; },
        [](auto)            { return false; }
    }, m_value);
}

bool variable::is_number() const {
    return std::visit(util::overloaded{
        [](number_t auto)   { return true; },
        [](auto)            { return false; }
    }, m_value);
}

bool variable::is_array() const {
    return std::visit(util::overloaded{
        [](std::vector<variable>) { return true; },
        [](auto) { return false; }
    }, m_value);
}

std::partial_ordering variable::operator <=> (const variable &other) const {
    return std::visit<std::partial_ordering>(util::overloaded{
        [&](string_t auto, string_t auto)           { return as_view() <=> other.as_view(); },

        [&](string_t auto, null_state)              { return as_view() <=> ""; },
        [&](null_state, string_t auto)              { return "" <=> other.as_view(); },
        
        [](number_t auto num1, number_t auto num2)  { return num1 <=> num2; },

        [&](string_t auto, number_t auto num)       { return as_number() <=> num; },
        [&](number_t auto num, string_t auto)       { return num <=> other.as_number(); },

        [](number_t auto num, null_state)           { return num <=> 0; },
        [](null_state, number_t auto num)           { return 0 <=> num; },
        
        [](datetime dt1, datetime dt2)              { return dt1 <=> dt2; },
        
        [&](datetime dt, string_t auto)             { return dt <=> other.as_date(); },
        [&](string_t auto, datetime dt)             { return as_date() <=> dt; },

        [](null_state, null_state)                  { return std::partial_ordering::equivalent; },
        [](auto, auto)                              { return std::partial_ordering::unordered; }
    }, m_value, other.m_value);
}

void variable::assign(const variable &other) {
    std::visit(util::overloaded{
        [&](auto) {
            m_str = other.m_str;
            m_value = other.m_value;
        },
        [&](std::string_view value) {
            m_str = value;
            m_value = string_state{};
        }
    }, other.m_value);
}

void variable::assign(variable &&other) {
    std::visit(util::overloaded{
        [&](auto) {
            m_str = std::move(other.m_str);
            m_value = std::move(other.m_value);
        },
        [&](std::string_view value) {
            m_str = value;
            m_value = string_state{};
        }
    }, other.m_value);
}

variable &variable::operator += (const variable &rhs) {
    std::visit(util::overloaded{
        [](null_state, null_state) {},
        [](auto, null_state) {},
        [&](null_state, auto) {
            *this = rhs;
        },
        [&](auto, auto) {
            get_string().append(rhs.as_view());
            m_value = string_state{};
        },
        [&](null_state, string_t auto) {
            m_str = rhs.as_view();
            m_value = string_state{};
        },
        [&](number_t auto num1, number_t auto num2) {
            *this = num1 + num2;
        },
        [&](number_t auto num1, fixed_point num2) {
            *this = fixed_point(num1) + num2;
        }
    }, m_value, rhs.m_value);
    return *this;
}

variable variable::operator +(const variable &rhs) const {
    variable copy = *this;
    return copy += rhs;
}

variable variable::operator -() const {
    return std::visit<variable>(util::overloaded{
        [](null_state) {
            return variable();
        },
        [](number_t auto n) {
            return -n;
        },
        [&](auto) {
            return -as_number();
        }
    }, m_value);
}

variable variable::operator -(const variable &rhs) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [&](not_number_t auto, fixed_point num) {
            return as_number() - num;
        },
        [&](not_number_t auto, big_int num) {
            return as_int() - num;
        },
        [&](not_number_t auto, double num) {
            return as_double() - num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) - num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 - num2;
        }
    }, m_value, rhs.m_value);
}

variable &variable::operator -= (const variable &rhs) {
    return *this = *this - rhs;
}

variable variable::operator * (const variable &rhs) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [&](not_number_t auto, fixed_point num) {
            return as_number() * num;
        },
        [&](not_number_t auto, big_int num) {
            return as_int() * num;
        },
        [&](not_number_t auto, double num) {
            return as_double() * num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) * num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 * num2;
        }
    }, m_value, rhs.m_value);
}

variable variable::operator / (const variable &rhs) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [&](not_number_t auto, fixed_point num) {
            return as_number() / num;
        },
        [&](not_number_t auto, big_int num) {
            return as_int() / num;
        },
        [&](not_number_t auto, double num) {
            return as_double() / num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) / num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 / num2;
        }
    }, m_value, rhs.m_value);
}