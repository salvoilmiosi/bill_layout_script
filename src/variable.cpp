#include "variable.h"

#include "utils.h"
#include "exceptions.h"

using namespace bls;

struct string_converter {
    std::string operator()(auto) const {
        return {};
    }
    std::string operator()(string_state str) const {
        return std::string(str);
    }
    std::string operator()(fixed_point num) const {
        return fixed_point_to_string(num);
    }
    std::string operator()(std::integral auto num) const {
        return std::to_string(num);
    }
    std::string operator()(std::floating_point auto num) const {
        return std::to_string(num);
    }
    std::string operator()(datetime date) const {
        return date.to_string();
    }
    std::string operator()(const variable_array &arr) const {
        return std::format("[{}]", util::string_join(arr | std::views::transform(&variable::as_view), ", "));
    }
};

const std::string &variable::as_string() const & {
    const auto &var = deref();
    if (!var.m_str) {
        var.m_str = std::make_unique<std::string>(std::visit(string_converter{}, var.m_value));
    }
    return *var.m_str;
}

std::string variable::as_string() && {
    if (auto *ptr = std::get_if<variable_ptr>(&m_value)) {
        return (*ptr)->as_string();
    } else if (m_str) {
        return std::move(*m_str);
    } else {
        return std::visit(string_converter{}, m_value);
    }
}

variable_array &variable::as_array() {
    return std::get<variable_array>(m_value);
}

const variable_array &variable::as_array() const {
    return std::get<variable_array>(deref().m_value);
}

string_state variable::as_view() const {
    if (auto *view = std::get_if<string_state>(&deref().m_value)) {
        return *view;
    } else {
        return std::string_view(as_string());
    }
}

template<typename T> concept convertible_from_string = requires(std::string_view str) {
    { util::string_to<T>(str) } -> std::convertible_to<T>;
};

template<typename T> struct basic_converter{
    T operator()(string_state str) const requires convertible_from_string<T> { return util::string_to<T>(str); }
    T operator()(const T &obj) const { return obj; }
    T operator()(auto) const { return T{}; }
};

template<typename T> struct number_converter_base : basic_converter<T> {
    using basic_converter<T>::operator();
    T operator()(std::integral auto num) const { return T(num); }
    T operator()(std::floating_point auto num) const { return T(num); }
};

template<typename T> struct number_converter {};

template<> struct number_converter<fixed_point> : number_converter_base<fixed_point> {
    using number_converter_base<fixed_point>::operator();
};
template<std::integral T> struct number_converter<T> : number_converter_base<T> {
    using number_converter_base<T>::operator();
    T operator()(fixed_point num) const { return num.getAsInteger(); }
};
template<std::floating_point T> struct number_converter<T> : number_converter_base<T> {
    using number_converter_base<T>::operator();
    T operator()(fixed_point num) const { return num.getAsDouble(); }
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
    return std::visit(basic_converter<datetime>{}, deref().m_value);
}

variable_ptr variable::as_pointer() const {
    if (auto *ptr = std::get_if<variable_ptr>(&m_value)) {
        return (*ptr)->as_pointer();
    } else {
        return this;
    }
}

const variable &variable::deref() const & {
    if (auto *ptr = std::get_if<variable_ptr>(&m_value)) {
        return (*ptr)->deref();
    } else {
        return *this;
    }
}

variable variable::deref() && {
    if (auto *ptr = std::get_if<variable_ptr>(&m_value)) {
        return (*ptr)->deref();
    } else {
        return std::move(*this);
    }
}

struct null_checker {
    bool operator()(auto) const             { return false; }
    bool operator()(std::monostate) const   { return true; }
    bool operator()(string_state str) const { return str.empty(); }
    bool operator()(datetime date) const    { return !date.is_valid(); }
    bool operator()(const variable_array &arr) const { return arr.empty(); }
};

bool variable::is_true() const {
    return std::visit(util::overloaded{
        [](const auto &value) { return !null_checker{}(value); },
        [](const number_t auto &num) { return num != 0; }
    }, deref().m_value);
}

bool variable::is_null() const {
    return std::visit(null_checker{}, deref().m_value);
}

bool variable::is_pointer() const {
    return std::holds_alternative<variable_ptr>(m_value);
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
    return std::holds_alternative<variable_array>(deref().m_value);
}

template<typename T> concept not_monostate = ! std::same_as<T, std::monostate>;

std::partial_ordering variable::operator <=> (const variable &other) const {
    return std::visit<std::partial_ordering>(util::overloaded{
        [] <typename Lhs, std::three_way_comparable_with<Lhs> Rhs> (const Lhs &lhs, const Rhs &rhs) {
            return lhs <=> rhs;
        },

        [] <not_monostate T> (const T &lhs, std::monostate) { return lhs <=> T{}; },
        [] <not_monostate T> (std::monostate, const T &rhs) { return T{} <=> rhs; },

        [] <convertible_from_string T> (const T &lhs, string_state rhs) { return lhs <=> util::string_to<T>(rhs); },
        [] <convertible_from_string T> (string_state lhs, const T &rhs) { return util::string_to<T>(lhs) <=> rhs; },

        [](const auto &, const auto &) { return std::partial_ordering::unordered; }
    }, deref().m_value, other.deref().m_value);
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

void variable::assign(const variable &other) {
    const auto &var = other.deref();
    if (auto *view = std::get_if<string_state>(&var.m_value)) {
        m_str = std::make_unique<std::string>(*view);
        m_value = string_state(*m_str, view->flags);
    } else {
        *this = var;
    }
}

void variable::assign(variable &&other) {
    if (auto *ptr = std::get_if<variable_ptr>(&other.m_value)) {
        assign((*ptr)->deref());
    } else if (auto *view = std::get_if<string_state>(&other.m_value)) {
        if (other.m_str) {
            m_str = std::move(other.m_str);
        } else {
            m_str = std::make_unique<std::string>(*view);
        }
        m_value = string_state(*m_str, view->flags);
    } else {
        *this = std::move(other);
    }
}

template<typename F>
struct operator_caller {
    variable operator()(auto, auto) const { return {}; }

    auto operator()(auto lhs, number_t auto rhs) const {
        return F{}(number_converter<decltype(rhs)>{}(lhs), rhs);
    }
    auto operator()(number_t auto lhs, auto rhs) const {
        return F{}(lhs, number_converter<decltype(lhs)>{}(rhs));
    }
    auto operator()(number_t auto lhs, fixed_point rhs) const {
        return F{}(fixed_point(lhs), rhs);
    }
    auto operator()(number_t auto lhs, number_t auto rhs) const {
        return F{}(lhs, rhs);
    }
};

template<typename T> constexpr bool is_string_state = std::is_same_v<std::decay_t<T>, string_state>;

variable &variable::operator += (const variable &other) {
    std::visit(util::overloaded{
        [this](const auto &lhs, const auto &rhs) {
            string_flags flags = as_string_tag;
            if constexpr (is_string_state<decltype(lhs)>) {
                if (!m_str) {
                    m_str = std::make_unique<std::string>(lhs);
                }
                flags = lhs.flags;
            } else if (!m_str) {
                m_str = std::make_unique<std::string>(string_converter{}(lhs));
            }
            if constexpr (is_string_state<decltype(rhs)>) {
                m_str->append(rhs);
            } else {
                m_str->append(string_converter{}(rhs));
            }
            m_value = string_state(*m_str, flags);
        },
        [this](const number_t auto &num1, const number_t auto &num2) {
            *this = operator_caller<std::plus<>>{}(num1, num2);
        },
        [this](variable_array &arr, const variable_array &rhs) {
            m_str.reset();
            arr.insert(arr.end(), rhs.begin(), rhs.end());
        },
        [this](std::monostate, const not_monostate auto &value) {
            assign(value);
        },
        [this](variable_ptr lhs, const not_monostate auto &rhs) {
            assign(*lhs + rhs);
        },
        [](auto, std::monostate) {},
    }, m_value, other.deref().m_value);
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
    return std::visit<variable>(operator_caller<std::minus<>>{}, deref().m_value, other.deref().m_value);
}

variable &variable::operator -= (const variable &other) {
    return *this = *this - other;
}

variable variable::operator * (const variable &other) const {
    return std::visit<variable>(operator_caller<std::multiplies<>>{}, deref().m_value, other.deref().m_value);
}

variable variable::operator / (const variable &other) const {
    return std::visit<variable>(operator_caller<std::divides<>>{}, deref().m_value, other.deref().m_value);
}