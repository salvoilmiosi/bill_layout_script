#ifndef __JSON_VALUE_H__
#define __JSON_VALUE_H__

#include <variant>
#include <vector>
#include <map>

#include "unicode.h"

namespace json {

    namespace detail {
        template<typename T, typename Var> struct type_in_variant_helper{};
        template<typename T, typename ... Ts> struct type_in_variant_helper<T, std::variant<Ts ...>> {
            static constexpr bool value = (std::is_convertible_v<T, Ts> || ...);
        };

        template<typename T, typename Var> concept type_in_variant = detail::type_in_variant_helper<T, Var>::value;
    };

    class value;

    using array = std::vector<value>;
    using object = std::map<std::string, value>;

    class value {
    private:
        using value_variant = std::variant<std::monostate, std::string, int, double, bool, array, object>;
        value_variant m_data;

    public:
        value() = default;
        value(const detail::type_in_variant<value_variant> auto &value) : m_data(value) {}
        value(detail::type_in_variant<value_variant> auto &&value) : m_data(std::move(value)) {}

        template<typename T> const T &as() const { return std::get<T>(m_data); }
        template<typename T> T &as() { return std::get<T>(m_data); }

        const array &as_array() const { return as<array>(); }
        array &as_array() { return as<array>(); }

        const object &as_object() const { return as<object>(); }
        object &as_object() { return as<object>(); }

        bool is_null() { return std::holds_alternative<std::monostate>(m_data); }

        template<typename StreamType>
        friend class printer;
    };
    
    template<typename StreamType>
    class printer {
    private:
        StreamType &stream;
        std::string indent;
        unsigned indent_size;

    public:
        printer(StreamType &stream, unsigned indent_size = 0)
            : stream(stream)
            , indent_size(indent_size) {}

        StreamType &operator()(const value &val) {
            return std::visit(*this, val.m_data);
        }

        StreamType &operator()(const auto &val) {
            return stream << val;
        }

        StreamType &operator()(const std::string &val) {
            return stream << unicode::escapeString(val);
        }

        StreamType &operator()(std::monostate) {
            return stream << "null";
        }

        StreamType &operator()(bool val) {
            if (val) return stream << "true";
            else return stream << "false";
        }

        StreamType &operator()(const array &arr) {
            stream << '[';
            if (indent_size > 0) {
                stream << '\n';
            }
            if (!arr.empty()) {
                indent.append(indent_size, ' ');
                auto it = arr.begin();
                for (;;) {
                    stream << indent;
                    (*this)(*it);
                    if (++it == arr.end()) break;
                    if (indent_size > 0) {
                        stream << ",\n";
                    } else {
                        stream << ", ";
                    }
                }
                indent.resize(indent.size() - indent_size);
            }
            if (indent_size > 0) {
                stream << '\n';
            }
            stream << indent << ']';
            return stream;
        }

        StreamType &operator()(const object &obj) {
            stream << '{';
            if (indent_size > 0) {
                stream << '\n';
            }
            if (!obj.empty()) {
                indent.append(indent_size, ' ');
                auto it = obj.begin();
                for (;;) {
                    stream << indent << unicode::escapeString(it->first) << ": ";
                    (*this)(it->second);
                    if (++it == obj.end()) break;
                    if (indent_size > 0) {
                        stream << ",\n";
                    } else {
                        stream << ", ";
                    }
                }
                indent.resize(indent.size() - indent_size);
            }
            if (indent_size > 0) {
                stream << '\n';
            }
            stream << indent << '}';
            return stream;
        }
    };

    template<typename StreamType>
    inline StreamType &operator << (StreamType &stream, const value &val) {
        return printer(stream)(val);
    }

    template<typename StreamType>
    inline StreamType &operator << (StreamType &stream, const array &arr) {
        return printer(stream)(arr);
    }

    template<typename StreamType>
    inline StreamType &operator << (StreamType &stream, const object &obj) {
        return printer(stream)(obj);
    }
};

#endif