#include "variable.h"

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;

std::string &variable::get_string() const {
    if (m_str.empty()) {
        m_str = std::visit(overloaded{
            [](std::monostate) {
                return std::string();
            },
            [&](std::string_view str) {
                return std::string(str);
            },
            [&](fixed_point num) {
                return fixed_point_to_string(num);
            },
            [&](wxDateTime date) {
                return date.FormatISODate().ToStdString();
            }
        }, m_data);
    }
    return m_str;
}

std::string_view variable::str_view() const {
    return std::visit(overloaded{
        [&](auto) {
            return std::string_view(str());
        },
        [](std::string_view str) {
            return str;
        }
    }, m_data);
}

fixed_point variable::number() const {
    return std::visit(overloaded{
        [&](auto) {
            return fixed_point(str());
        },
        [](fixed_point num) {
            return num;
        }
    }, m_data);
}

wxDateTime variable::date() const {
    return std::visit(overloaded{
        [&](auto) {
            wxDateTime dt(time_t(0));
            dt.ParseISODate(str());
            return dt;
        },
        [](wxDateTime date) {
            return date;
        }
    }, m_data);
}

bool variable::as_bool() const {
    return std::visit(overloaded{
        [&](std::monostate) {
            return !m_str.empty();
        },
        [](std::string_view str) {
            return !str.empty();
        },
        [](fixed_point num) {
            return num != fixed_point(0);
        },
        [](wxDateTime date) {
            return date.GetTicks() != 0;
        }
    }, m_data);
}

bool variable::empty() const {
    return std::visit(overloaded{
        [&](std::monostate) {
            return m_str.empty();
        },
        [](std::string_view str) {
            return str.empty();
        },
        [](fixed_point num) {
            return false;
        },
        [](wxDateTime date) {
            return date.GetTicks() == 0;
        }
    }, m_data);
}

template<typename T>
concept is_string = std::is_same_v<T, std::string_view> || std::is_same_v<T, std::monostate>;

inline auto operator <=> (const wxDateTime &lhs, const wxDateTime &rhs) {
    return lhs.GetTicks() <=> rhs.GetTicks();
}

std::partial_ordering variable::operator <=> (const variable &other) const {
    return std::visit<std::partial_ordering>(overloaded{
        [&](is_string auto, is_string auto)     { return str_view() <=> other.str_view(); },
        [&](is_string auto, fixed_point num)    { return number() <=> num; },
        [&](fixed_point num, is_string auto)    { return num <=> other.number(); },
        [](fixed_point num1, fixed_point num2)  { return num1 <=> num2; },
        [&](is_string auto, wxDateTime dt)      { return date() <=> dt; },
        [&](wxDateTime dt, is_string auto)      { return dt <=> date(); },
        [](wxDateTime dt1, wxDateTime dt2)      { return dt1 <=> dt2; },
        [](auto, auto) { return std::partial_ordering::unordered; }
    }, m_data, other.m_data);
}

void variable::assign(const variable &other) {
    std::visit(overloaded{
        [&](auto) {
            m_str = other.m_str;
            m_data = other.m_data;
        },
        [&](std::string_view value) {
            m_str = value;
            m_data = std::monostate{};
        }
    }, other.m_data);
}

void variable::assign(variable &&other) {
    std::visit(overloaded{
        [&](auto) {
            m_str = std::move(other.m_str);
            m_data = std::move(other.m_data);
        },
        [&](std::string_view value) {
            m_str = value;
            m_data = std::monostate{};
        }
    }, other.m_data);
}

void variable::append(const variable &other) {
    std::visit(overloaded{
        [&](auto) {
            get_string().append(other.str());
            m_data = std::monostate{};
        },
        [&](std::string_view str) {
            get_string().append(str);
            m_data = std::monostate{};
        },
        [&](fixed_point num) {
            m_data = number() + num;
            m_str.clear();
        }
    }, other.m_data);
}