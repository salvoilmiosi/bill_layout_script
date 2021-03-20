#include "variable.h"

#include "utils.h"

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;

template<typename T, typename ... Ts> struct is_one_of : std::false_type {};
template<typename T, typename First, typename ... Ts>
struct is_one_of<T, First, Ts...> : std::bool_constant<is_one_of<T, Ts...>::value> {};
template<typename T, typename ... Ts> struct is_one_of<T, T, Ts...> : std::true_type {};
template<typename T, typename ... Ts> constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

template<typename T> concept string_t = is_one_of_v<T, std::string_view, std::monostate>;
template<typename T> concept number_t = is_one_of_v<T, fixed_point, int64_t>;

std::string &variable::get_string() const {
    if (m_str.empty()) {
        m_str = std::visit(overloaded{
            [](std::monostate) {
                return std::string();
            },
            [](std::string_view str) {
                return std::string(str);
            },
            [](fixed_point num) {
                return fixed_point_to_string(num);
            },
            [](int64_t num) {
                return num_tostring(num);
            },
            [](wxDateTime date) {
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
        [](number_t auto num) {
            return fixed_point(num);
        }
    }, m_data);
}

int64_t variable::as_int() const {
    return std::visit(overloaded{
        [&](auto) {
            int64_t num = 0;
            auto view = str_view();
            std::from_chars(view.begin(), view.end(), num);
            return num;
        },
        [](fixed_point num) {
            return num.getAsInteger();
        },
        [](int64_t num) {
            return num;
        }
    }, m_data);
}

double variable::as_double() const {
    return std::visit(overloaded{
        [&](auto) {
            return number().getAsDouble();
        },
        [](int64_t num) {
            return double(num);
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
        [](int64_t num) {
            return num != 0;
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
        [](number_t auto) {
            return false;
        },
        [](wxDateTime date) {
            return date.GetTicks() == 0;
        }
    }, m_data);
}

bool variable::is_string() const {
    return std::visit(overloaded{
        [](string_t auto) {
            return true;
        },
        [](auto) {
            return false;
        }
    }, m_data);
}

bool variable::is_number() const {
    return std::visit(overloaded{
        [](number_t auto) {
            return true;
        },
        [](auto) {
            return false;
        }
    }, m_data);
}

inline auto operator <=> (const wxDateTime &lhs, const wxDateTime &rhs) {
    return lhs.GetTicks() <=> rhs.GetTicks();
}

std::partial_ordering variable::operator <=> (const variable &other) const {
    return std::visit<std::partial_ordering>(overloaded{
        [&](string_t auto, string_t auto)           { return str_view() <=> other.str_view(); },
        [&](string_t auto, number_t auto num)       { return number() <=> num; },
        [&](number_t auto num, string_t auto)       { return num <=> other.number(); },
        [](number_t auto num1, number_t auto num2)  { return num1 <=> num2; },
        [&](string_t auto, wxDateTime dt)           { return date() <=> dt; },
        [&](wxDateTime dt, string_t auto)           { return dt <=> other.date(); },
        [](wxDateTime dt1, wxDateTime dt2)          { return dt1 <=> dt2; },
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
        [&](auto, auto) {
            get_string().append(other.str_view());
            m_data = std::monostate{};
        },
        [&](auto, fixed_point num) {
            m_data = number() + num;
            m_str.clear();
        },
        [&](auto, int64_t num) {
            m_data = as_int() + num;
            m_str.clear();
        },
        [&](number_t auto num1, number_t auto num2) {
            m_data = num1 + num2;
            m_str.clear();
        },
    }, m_data, other.m_data);
}