#include "variable.h"

using namespace bls;

template<typename T> static std::string variable_type_label() {
    return intl::enum_label(enums::enum_variant_indexof_v<T, variable_variant>);
}

template<typename T1, typename T2>
static auto make_conversion_error() {
    return bls::conversion_error(intl::translate("CANT_CONVERT_TYPE_TO_TYPE", variable_type_label<T1>(), variable_type_label<T2>()));
}

struct string_converter {
    std::string operator()(auto value) const {
        throw make_conversion_error<decltype(value), string_state>();
    }
    std::string operator()(std::monostate) const {
        return std::string();
    }
    std::string operator()(string_state str) const {
        return std::string(str);
    }
    std::string operator()(fixed_point num) const {
        return decimal_to_string(num);
    }
    std::string operator()(bool value) const {
        return value ? "true" : "false";
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
        var.m_str = std::make_unique<std::string>(enums::visit(string_converter{}, var.m_value));
    }
    return *var.m_str;
}

std::string variable::as_string() && {
    if (auto *ptr = std::get_if<variable_ptr>(&m_value)) {
        return (*ptr)->as_string();
    } else if (m_str) {
        return std::move(*m_str);
    } else {
        return enums::visit(string_converter{}, m_value);
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
    T operator()(auto value) const { throw make_conversion_error<decltype(value), T>(); }
    T operator()(std::monostate) const requires std::default_initializable<T> { return T{}; }
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
    return enums::visit(number_converter<fixed_point>{}, deref().m_value);
}

int64_t variable::as_int() const {
    return enums::visit(number_converter<int64_t>{}, deref().m_value);
}

double variable::as_double() const {
    return enums::visit(number_converter<double>{}, deref().m_value);
}

datetime variable::as_date() const {
    return enums::visit(basic_converter<datetime>{}, deref().m_value);
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

struct size_getter {
    size_t operator()(auto) const               { return 1; }
    size_t operator()(std::monostate) const     { return 0; }
    size_t operator()(string_state str) const   { return str.size(); }
    size_t operator()(const variable_array &arr) const { return arr.size(); }
};

size_t variable::size() const {
    return enums::visit(size_getter{}, deref().m_value);
}

bool variable::is_empty() const {
    return enums::visit(std::not_fn(size_getter{}), deref().m_value);
}

bool variable::is_true() const {
    return enums::visit(util::overloaded{
        [](const auto &value) { return size_getter{}(value) != 0; },
        [](const number_t auto &num) { return num != 0; }
    }, deref().m_value);
}

bool variable::is_null() const {
    return std::holds_alternative<std::monostate>(m_value);
}

bool variable::is_pointer() const {
    return std::holds_alternative<variable_ptr>(m_value);
}

bool variable::is_number() const {
    return enums::visit(util::overloaded{
        [](number_t auto)   { return true; },
        [](auto)            { return false; }
    }, deref().m_value);
}

bool variable::is_array() const {
    return std::holds_alternative<variable_array>(deref().m_value);
}

template<typename T> concept not_monostate = ! std::same_as<T, std::monostate>;

template<typename T> concept comparable_with_string = convertible_from_string<T>
    && std::three_way_comparable<T>;

template<typename T, typename U> concept simple_comparable_with = requires(T lhs, U rhs) {
    lhs <=> rhs;
};

template<typename T> concept comparable_with_null = not_monostate<T> &&
    std::default_initializable<T> && std::three_way_comparable<T>;

struct variable_comparator {
    template<typename Lhs, typename Rhs>
    std::partial_ordering operator()(const Lhs &, const Rhs &) const {
        throw make_conversion_error<Lhs, Rhs>();
    }

    template<typename Lhs, simple_comparable_with<Lhs> Rhs>
    std::partial_ordering operator()(const Lhs &lhs, const Rhs &rhs) const {
        return lhs <=> rhs;
    }

    std::partial_ordering operator()(std::monostate, const not_monostate auto &) {
        return std::partial_ordering::unordered;
    }

    std::partial_ordering operator()(const not_monostate auto &, std::monostate) {
        return std::partial_ordering::unordered;
    }

    template<comparable_with_null Rhs>
    std::partial_ordering operator()(std::monostate, const Rhs &rhs) {
        return Rhs{} <=> rhs;
    }

    template<comparable_with_null Lhs>
    std::partial_ordering operator()(const Lhs &lhs, std::monostate) {
        return lhs <=> Lhs{};
    }

    template<comparable_with_string T>
    std::partial_ordering operator()(string_state lhs, const T &rhs) {
        return util::string_to<T>(lhs) <=> rhs;
    }

    template<comparable_with_string T>
    std::partial_ordering operator()(const T &lhs, string_state rhs) {
        return lhs <=> util::string_to<T>(rhs);
    }
};

std::partial_ordering variable::operator <=> (const variable &other) const {
    return enums::visit(variable_comparator{}, deref().m_value, other.deref().m_value);
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
    enums::visit(util::overloaded{
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
    return enums::visit<variable>(util::overloaded{
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
    return enums::visit<variable>(operator_caller<std::minus<>>{}, deref().m_value, other.deref().m_value);
}

variable &variable::operator -= (const variable &other) {
    return *this = *this - other;
}

variable variable::operator * (const variable &other) const {
    return enums::visit<variable>(operator_caller<std::multiplies<>>{}, deref().m_value, other.deref().m_value);
}

variable variable::operator / (const variable &other) const {
    return enums::visit<variable>(operator_caller<std::divides<>>{}, deref().m_value, other.deref().m_value);
}