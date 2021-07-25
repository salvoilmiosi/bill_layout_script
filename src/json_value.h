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

        template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
        template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts ...>;
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

        bool is_null() const { return std::holds_alternative<std::monostate>(m_data); }

        size_t size() const {
            return std::visit<size_t>(detail::overloaded{
                [](auto) { return 1; },
                [](std::monostate) { return 0; },
                [](const array &arr) { return arr.size(); },
                [](const object &obj) { return obj.size(); }
            }, m_data);
        }

        void push_back(const value &val) { as_array().push_back(val); }
        void push_back(value &&val) { as_array().push_back(std::move(val)); }

        template<typename ... Ts> value &emplace_back(Ts && ... args) {
            return as_array().emplace_back(std::forward<Ts>(args) ... );
        }

        const value &operator[](size_t index) const { return as_array()[index]; }
        value &operator[](size_t index) { return as_array()[index]; }

        const value &operator[](const std::string &key) const { return as_object().at(key); }
        value &operator[](const std::string &key) { return as_object()[key]; }

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
                for (auto it = arr.begin();;) {
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
                for (auto it = obj.begin();;) {
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