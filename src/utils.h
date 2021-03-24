#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <algorithm>
#include <charconv>

#include <fmt/format.h>

typedef uint8_t small_int;

constexpr char UNIT_SEPARATOR = '\x1f';

struct hasher {
    constexpr size_t operator() (const char *begin, const char *end) const {
        return begin != end ? static_cast<unsigned int>(*begin) + 33 * (*this)(begin + 1, end) : 5381;
    }
    
    constexpr size_t operator() (std::string_view str) const {
        return (*this)(str.begin(), str.end());
    }
};

// restituisce l'hash di una stringa
template<typename T> size_t constexpr hash(T&& t) {
    return hasher{}(std::forward<T>(t));
}

// sposta il range indicato alla fine
template<typename T>
void move_to_end(T &vec, size_t begin, size_t end) {
    for (size_t i=begin; i<end; ++i) {
        vec.push_back(vec[i]);
    }
    vec.erase(vec.begin() + begin, vec.begin() + end);
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
        throw std::invalid_argument(fmt::format("Impossibile convertire {} in numero", str));
    }
    return ret;
}

constexpr int string_toint(std::string_view str) {
    return string_to<int>(str);
}

constexpr std::string num_tostring(auto num) {
    std::array<char, 16> buf;
    auto [ptr, ec] = std::to_chars(buf.begin(), buf.end(), num);
    if (ec == std::errc()) {
        return {buf.begin(), ptr};
    }
    return {};
}

#endif