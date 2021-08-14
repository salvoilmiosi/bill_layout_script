#include "variable.h"

#include "utils.h"
#include "exceptions.h"

using namespace bls;

std::string &variable::get_string() const {
    if (!m_str) {
        m_str = std::make_unique<std::string>(std::visit(util::overloaded{
            [](auto)                    { return std::string(); },
            [](string_state str)        { return std::string(str); },
            [](fixed_point num)         { return fixed_point_to_string(num); },
            [](std::integral auto num)  { return std::to_string(num); },
            [](std::floating_point auto num) { return std::to_string(num); },
            [](datetime date)           { return date.to_string(); },
            [](const std::vector<variable> &arr) {
                return std::format("[{}]", util::string_join(arr | std::views::transform(&variable::as_view), ", "));
            }
        }, m_value));
    }
    return *m_str;
}

const std::string &variable::as_string() const & {
    return deref().get_string();
}

std::string variable::as_string() && {
    if (auto *ptr = std::get_if<const variable *>(&m_value)) {
        return (*ptr)->as_string();
    } else {
        return std::move(get_string());
    }
}

std::vector<variable> &variable::as_array() {
    return std::get<std::vector<variable>>(m_value);
}

const std::vector<variable> &variable::as_array() const {
    return std::get<std::vector<variable>>(deref().m_value);
}

string_state variable::as_view() const {
    if (auto *view = std::get_if<string_state>(&deref().m_value)) {
        return *view;
    } else {
        return std::string_view(as_string());
    }
}

template<typename T> struct number_converter{};

template<> struct number_converter<fixed_point> {
    fixed_point operator()(string_state str) const { return util::string_to<fixed_point>(str); }
    fixed_point operator()(std::integral auto num) const { return fixed_point(num); }
    fixed_point operator()(std::floating_point auto num) const { return fixed_point(num); }
    fixed_point operator()(fixed_point num) const { return num; }
    fixed_point operator()(auto) const { return {}; }
};

template<std::integral T> struct number_converter<T> {
    T operator()(string_state str) const { return util::string_to<T>(str); }
    T operator()(fixed_point num) const { return num.getAsInteger(); }
    T operator()(std::convertible_to<T> auto num) const { return num; }
    T operator()(auto) const { return {}; }
};

template<std::floating_point T> struct number_converter<T> {
    T operator()(string_state str) const { return util::string_to<T>(str); }
    T operator()(fixed_point num) const { return num.getAsDouble(); }
    T operator()(std::convertible_to<T> auto num) const { return num; }
    T operator()(auto) const { return {}; }
};

template<typename T> concept number_t = std::invocable<number_converter<T>, string_state>;

fixed_point variable::as_number() const {
    return std::visit(number_converter<fixed_point>{}, deref().m_value);
}

big_int variable::as_int() const {
    return std::visit(number_converter<big_int>{}, deref().m_value);
}

double variable::as_double() const {
    return std::visit(number_converter<double>{}, deref().m_value);
}

datetime variable::as_date() const {
    return std::visit(util::overloaded{
        [](string_state str)    { return datetime::from_string(str); },
        [](datetime date)       { return date; },
        [](auto)                { return datetime(); },
    }, deref().m_value);
}

bool variable::as_bool() const {
    return std::visit(util::overloaded{
        [](std::monostate)      { return false; },
        [](auto)                { return true; },
        [](string_state str)    { return !str.empty(); },
        [](number_t auto num)   { return num != 0; },
        [](datetime date)       { return date.is_valid(); },
        [](const std::vector<variable> &arr) { return !arr.empty(); }
    }, deref().m_value);
}

const variable *variable::as_pointer() const {
    if (auto *ptr = std::get_if<const variable *>(&m_value)) {
        return (*ptr)->as_pointer();
    } else {
        return this;
    }
}

const variable &variable::deref() const & {
    if (auto *ptr = std::get_if<const variable *>(&m_value)) {
        return (*ptr)->deref();
    } else {
        return *this;
    }
}

variable variable::deref() && {
    if (auto *ptr = std::get_if<const variable *>(&m_value)) {
        return (*ptr)->deref();
    } else {
        return std::move(*this);
    }
}

bool variable::is_null() const {
    return std::visit(util::overloaded{
        [](std::monostate)      { return true; },
        [](auto)                { return false; },
        [](string_state str)    { return str.empty(); },
        [](datetime date)       { return !date.is_valid(); },
        [](const std::vector<variable> &arr) { return arr.empty(); }
    }, deref().m_value);
}

bool variable::is_pointer() const {
    return std::holds_alternative<const variable *>(m_value);
}

bool variable::is_string() const {
    return std::holds_alternative<string_state>(deref().m_value);
}

bool variable::is_regex() const {
    auto *view = std::get_if<string_state>(&deref().m_value);
    return view && view->flags.is_regex;
}

bool variable::is_view() const {
    const auto &var = deref();
    return std::holds_alternative<string_state>(var.m_value) && ! var.m_str;
}

bool variable::is_number() const {
    return std::visit(util::overloaded{
        [](number_t auto)   { return true; },
        [](auto)            { return false; }
    }, deref().m_value);
}

bool variable::is_array() const {
    return std::holds_alternative<std::vector<variable>>(deref().m_value);
}

std::partial_ordering variable::operator <=> (const variable &other) const {
    return std::visit<std::partial_ordering>(util::overloaded{
        [](string_state lhs, string_state rhs)      { return lhs <=> rhs; },

        [](string_state lhs, std::monostate)        { return lhs <=> ""; },
        [](std::monostate, string_state rhs)        { return "" <=> rhs; },
        
        [](number_t auto num1, number_t auto num2)  { return num1 <=> num2; },

        [](string_state lhs, number_t auto num)     { return util::string_to<fixed_point>(lhs) <=> num; },
        [](number_t auto num, string_state rhs)     { return num <=> util::string_to<fixed_point>(rhs); },

        [](number_t auto num, std::monostate)       { return num <=> 0; },
        [](std::monostate, number_t auto num)       { return 0 <=> num; },
        
        [](datetime dt1, datetime dt2)              { return dt1 <=> dt2; },
        
        [](datetime dt, string_state str)           { return dt <=> datetime::from_string(str); },
        [](string_state str, datetime dt)           { return datetime::from_string(str) <=> dt; },

        [](std::monostate, std::monostate)          { return std::partial_ordering::equivalent; },
        [](auto, auto)                              { return std::partial_ordering::unordered; }
    }, deref().m_value, other.deref().m_value);
}

variable::variable(const variable &other) {
    *this = other;
}

variable::variable(variable &&other) {
    *this = std::move(other);
}

variable &variable::operator = (const variable &other) {
    if (auto *view = std::get_if<string_state>(&other.m_value); view && other.m_str) {
        m_str = std::make_unique<std::string>(*view);
        m_value = string_state(*m_str, view->flags);
    } else {
        m_str.reset();
        m_value = other.m_value;
    }
    return *this;
}

variable &variable::operator = (variable &&other) {
    if (auto *view = std::get_if<string_state>(&other.m_value); view && other.m_str) {
        m_str = std::move(other.m_str);
        m_value = string_state(*m_str, view->flags);
    } else {
        m_str.reset();
        m_value = std::move(other.m_value);
    }
    return *this;
}

void variable::assign(const variable &other) {
    std::visit(util::overloaded{
        [&](auto) {
            m_str.reset();
            m_value = other.m_value;
        },
        [&](string_state view) {
            m_str = std::make_unique<std::string>(view);
            m_value = string_state(*m_str, view.flags);
        },
        [&](const variable *ptr) {
            *this = *ptr;
        }
    }, other.m_value);
}

void variable::assign(variable &&other) {
    std::visit(util::overloaded{
        [&](auto) {
            m_str.reset();
            m_value = std::move(other.m_value);
        },
        [&](string_state view) {
            if (other.m_str) {
                m_str = std::move(other.m_str);
            } else {
                m_str = std::make_unique<std::string>(view);
            }
            m_value = string_state(*m_str, view.flags);
        },
        [&](const variable *ptr) {
            assign(*ptr);
        }
    }, other.m_value);
}

variable &variable::operator += (const variable &other) {
    std::visit(util::overloaded{
        [](std::monostate, std::monostate) {},
        [](auto, std::monostate) {},
        [](const variable *, std::monostate) {},
        [&](std::monostate, auto) {
            *this = other;
        },
        [&](std::monostate, const variable *rhs) {
            *this = *rhs;
        },
        [&](auto, auto) {
            get_string().append(other.as_view());
            m_value = string_state(*m_str);
        },
        [&](std::monostate, string_state view) {
            m_str = std::make_unique<std::string>(view);
            m_value = string_state(*m_str, view.flags);
        },
        [&](number_t auto num1, number_t auto num2) {
            *this = num1 + num2;
        },
        [&](number_t auto num1, fixed_point num2) {
            *this = fixed_point(num1) + num2;
        },
        [&](const variable *lhs, auto) {
            *this = *lhs + other;
        },
        [&](auto, const variable *rhs) {
            *this += *rhs;
        },
        [&](const variable *lhs, const variable *rhs) {
            *this = *lhs + *rhs;
        }
    }, m_value, other.m_value);
    return *this;
}

variable variable::operator +(const variable &rhs) const {
    variable copy = *this;
    return copy += rhs;
}

variable variable::operator -() const {
    return std::visit<variable>(util::overloaded{
        [](std::monostate) {
            return variable();
        },
        [](number_t auto n) {
            return -n;
        },
        [](auto v) {
            return -number_converter<fixed_point>{}(v);
        }
    }, deref().m_value);
}

variable variable::operator -(const variable &other) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [](auto lhs, number_t auto num) {
            return number_converter<decltype(num)>{}(lhs) - num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) - num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 - num2;
        }
    }, deref().m_value, other.deref().m_value);
}

variable &variable::operator -= (const variable &rhs) {
    return *this = *this - rhs;
}

variable variable::operator * (const variable &other) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [](auto lhs, number_t auto num) {
            return number_converter<decltype(num)>{}(lhs) * num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) * num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 * num2;
        }
    }, deref().m_value, other.deref().m_value);
}

variable variable::operator / (const variable &other) const {
    return std::visit<variable>(util::overloaded{
        [](auto, auto) {
            return variable();
        },
        [](auto lhs, number_t auto num) {
            return number_converter<decltype(num)>{}(lhs) / num;
        },
        [](number_t auto num1, fixed_point num2) {
            return fixed_point(num1) / num2;
        },
        [](number_t auto num1, number_t auto num2) {
            return num1 / num2;
        }
    }, deref().m_value, other.deref().m_value);
}