#ifndef __STRING_PTR_H__
#define __STRING_PTR_H__

#include <set>
#include <string>

class string_ptr {
private:
    inline static std::set<std::string, std::less<>> m_data;
    const std::string *m_value;

    template<typename U>
    auto find_string(U && str) {
        auto it = m_data.lower_bound(str);
        if (*it != str) {
            it = m_data.emplace_hint(it, std::forward<U>(str));
        }
        return &*it;
    }

public:
    string_ptr() : m_value(find_string("")) {}
    string_ptr(std::string_view str) : m_value(find_string(str)) {}
    string_ptr(const std::string &str) : m_value(find_string(str)) {}
    string_ptr(std::string &&str) : m_value(find_string(std::move(str))) {}

    const std::string &operator *() const {
        return *m_value;
    }

    const std::string *operator ->() const {
        return m_value;
    }

    auto operator <=> (const string_ptr &rhs) const {
        return m_value <=> rhs.m_value;
    }
};

#endif