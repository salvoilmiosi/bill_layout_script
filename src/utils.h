#ifndef __UTILS_H__
#define __UTILS_H__

#include <wx/datetime.h>

#include <string>
#include <algorithm>
#include <charconv>
#include <ranges>

#ifndef HAVE_STD_FORMAT
#include <fmt/format.h>
namespace std {
    template<typename ... Ts>
    inline auto format(Ts && ... args) {
        return fmt::format(std::forward<Ts>(args) ...);
    }
}
#else
#include <format>
#endif


constexpr char UNIT_SEPARATOR = '\x1f';

template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts...>;

// restituisce l'hash di una stringa
constexpr size_t hash(std::string_view str) {
    size_t ret = 5381;
    for (size_t ch : str) {
        ret = ret * 33 + ch;
    }
    return ret;
}

// divide una stringa per separatore
template<std::ranges::input_range R>
inline auto string_split(std::string_view str, R &&separator) {
    return str
        | std::views::split(separator)
        | std::views::transform([](auto && range) {
            return std::string_view(&*range.begin(), std::ranges::distance(range));
        });
}

inline auto string_split(std::string_view str, char separator = ',') {
    return string_split(str, std::views::single(separator));
}

// unisce tutte le stringhe in un range di stringhe
template<std::ranges::input_range R>
std::string string_join(R &&vec, auto separator) {
    if (vec.empty()) return "";
    auto it = vec.begin();
    std::string ret(*it);
    for (++it; it != vec.end(); ++it) {
        ret += separator;
        ret += *it;
    }
    return ret;
};

template<std::ranges::input_range R>
std::string string_join(R &&vec) {
    std::string ret;
    for (const auto &c : vec | std::views::join) {
        ret += c;
    }
    return ret;
}

// elimina gli spazi in eccesso a inizio e fine stringa
inline std::string string_trim(std::string_view str) {
    auto view = str
        | std::views::drop_while(isspace)
        | std::views::reverse
        | std::views::drop_while(isspace)
        | std::views::reverse;
    return {view.begin(), view.end()};
}

// sostituisce tutte le occorrenze di una stringa in un'altra
inline std::string &string_replace(std::string &str, std::string_view from, std::string_view to) {
    size_t index = 0;
    while (true) {
        index = str.find(from, index);
        if (index == std::string::npos) break;

        str.replace(index, from.size(), to);
        index += to.size();
    }
    return str;
}

template<typename T>
constexpr T string_to(std::string_view str) {
    T ret;
    auto result = std::from_chars(str.begin(), str.end(), ret);
    if (result.ec != std::errc()) {
        throw std::invalid_argument(std::format("Impossibile convertire {} in numero", str));
    }
    return ret;
}

template<typename T> requires std::is_arithmetic_v<T>
std::string num_tostring(T num) {
    return std::format("{}", num);
}

inline auto operator <=> (const wxLongLong &lhs, const wxLongLong &rhs) {
    return lhs.GetValue() <=> rhs.GetValue();
}

inline auto operator <=> (const wxDateTime &lhs, const wxDateTime &rhs) {
    return lhs.GetValue() <=> rhs.GetValue();
}

#endif