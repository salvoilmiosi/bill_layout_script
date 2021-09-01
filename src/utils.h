#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <algorithm>
#include <charconv>
#include <ranges>
#include <map>

#include "translations.h"
#include "svstream.h"
#include "exceptions.h"
#include "contig_split_view.h"
#include "static_vector.h"
#include "static_map.h"
#include "simple_stack.h"

namespace util {

    template<typename ... Ts> struct overloaded : Ts ... { using Ts::operator() ...; };
    template<typename ... Ts> overloaded(Ts ...) -> overloaded<Ts...>;

    template<typename T> struct strong_typedef : T {
        using T::T;
        strong_typedef(const T &x) : T(x) {}
        strong_typedef(T &&x) : T(std::move(x)) {}
    };

#ifdef USE_UNORDERED_MAP
    struct string_hash {
        using hash_type = std::hash<std::string_view>;
        using is_transparent = void;
    
        size_t operator()(const char* str) const        { return hash_type{}(str); }
        size_t operator()(std::string_view str) const   { return hash_type{}(str); }
        size_t operator()(std::string const& str) const { return hash_type{}(str); }
    };

    template<typename T> using string_map = std::unordered_map<std::string, T, string_hash, std::equal_to<>>;
#else
    template<typename T> using string_map = std::map<std::string, T, std::less<>>;
#endif

    template<typename Container> struct range_to_tag {};
    template<typename Container> static constexpr range_to_tag<Container> range_to;

    template<std::ranges::input_range R, typename Container>
    inline Container operator | (R &&range, range_to_tag<Container>) {
        return {range.begin(), range.end()};
    }

    struct range_to_vector_tag {};
    static constexpr range_to_vector_tag range_to_vector;

    template<std::ranges::input_range R>
    inline auto operator | (R &&range, range_to_vector_tag) {
        return range | range_to<std::vector<std::ranges::range_value_t<R>>>;
    }

    // divide una stringa per separatore
    template<std::ranges::input_range R>
    inline auto string_split(std::string_view str, R &&separator) {
        return str | std::views::split(separator);
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
        return str
            | std::views::drop_while(isspace)
            | std::views::reverse
            | std::views::drop_while(isspace)
            | std::views::reverse
            | util::range_to<std::string>;
    }

    // sostituisce tutte le occorrenze di una stringa in un'altra
    inline std::string string_replace(std::string_view str, std::string_view from, std::string_view to) {
        std::string ret(str);
        size_t index = 0;
        while (true) {
            index = ret.find(from, index);
            if (index == std::string::npos) break;

            ret.replace(index, from.size(), to);
            index += to.size();
        }
        return ret;
    }

    template<typename T> T string_to(std::string_view str) = delete;

    template<typename T> concept from_chars_readable = requires(T x, std::string_view str) {
        std::from_chars(str.data(), str.data() + str.size(), x);
    };
    
    template<std::floating_point T> requires (! from_chars_readable<T>)
    T string_to(std::string_view str) {
        T ret;
        isviewstream ss{str};
        ss >> ret;
        if (ss.fail()) {
            throw bls::conversion_error(intl::translate("CANT_PARSE_NUMBER", str));
        }
        return ret;
    }

    template<from_chars_readable T>
    constexpr T string_to(std::string_view str) {
        T ret;
        auto result = std::from_chars(str.data(), str.data() + str.size(), ret);
        if (result.ec != std::errc()) {
            throw bls::conversion_error(intl::translate("CANT_PARSE_NUMBER", str));
        }
        return ret;
    }

}

#endif