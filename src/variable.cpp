#include "variable.h"

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;

variable &variable::operator = (const variable &other) {
    std::visit(overloaded{
        [&](const auto &value) {
            m_str = other.m_str;
            m_data = value;
        },
        [&](std::string_view value) {
            m_str = value;
            m_data = std::monostate{};
        }
    }, other.m_data);
    return *this;
}

variable &variable::operator = (variable &&other) noexcept {
    std::visit(overloaded{
        [&](auto &&value) {
            m_str = std::move(other.m_str);
            m_data = std::move(value);
        },
        [&](std::string_view value) {
            m_str = value;
            m_data = std::monostate{};
        }
    }, other.m_data);
    return *this;
}

std::string &variable::get_string() const {
    std::visit(overloaded{
        [](std::monostate) {},
        [&](std::string_view str) {
            m_str = str;
        },
        [&](fixed_point num) {
            m_str = fixed_point_to_string(num);
        },
        [&](wxDateTime date) {
            m_str = date.FormatISODate().ToStdString();
        }
    }, m_data);
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

std::partial_ordering variable::operator <=> (const variable &other) const {
    using ord = std::partial_ordering;
    return std::visit(overloaded{
        [&](std::monostate) {
            return std::visit(overloaded{
                [&](std::monostate)         -> ord { return m_str <=> other.m_str; },
                [&](std::string_view str)   -> ord { return m_str <=> str; },
                [&](fixed_point num)        -> ord { return number().getUnbiased() <=> num.getUnbiased(); },
                [&](wxDateTime dt)          -> ord { return date().GetTicks() <=> dt.GetTicks(); }
            }, other.m_data);
        },
        [&](std::string_view str) {
            return std::visit(overloaded{
                [&](std::string_view str2)  -> ord { return str <=> str2; },
                [&](std::monostate)         -> ord { return str <=> other.m_str; },
                [](auto)                    -> ord { return ord::unordered; },
            }, other.m_data);
        },
        [&](fixed_point num) {
            return std::visit(overloaded{
                [&](fixed_point num2)       -> ord { return num.getUnbiased() <=> num2.getUnbiased(); },
                [&](std::monostate)         -> ord { return num.getUnbiased() <=> other.number().getUnbiased(); },
                [](auto)                    -> ord { return ord::unordered; }
            }, other.m_data);
        },
        [&](wxDateTime date) {
            return std::visit(overloaded{
                [&](wxDateTime date2)       -> ord { return date.GetTicks() <=> date2.GetTicks(); },
                [&](std::monostate)         -> ord { return date.GetTicks() <=> other.date().GetTicks(); },
                [](auto)                    -> ord { return ord::unordered; },
            }, other.m_data);
        },
    }, m_data);
}

variable &variable::operator += (const variable &other) {
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
    return *this;
}