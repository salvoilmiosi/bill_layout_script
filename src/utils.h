#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <algorithm>
#include <numeric>
#include <charconv>

#include <wx/datetime.h>

#include "functions.h"
#include "intl.h"

constexpr std::string_view RESULT_SEPARATOR = "\x1f";

typedef uint8_t small_int;
typedef uint8_t flags_t;

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
inline auto string_split(std::string_view str, std::string_view separator = ",") {
    return str
        | std::views::split(separator)
        | std::views::transform([](auto && range) {
            return std::string_view(&*range.begin(), std::ranges::distance(range));
        });
}

// unisce tutte le stringhe in un range di stringhe
template<std::ranges::input_range R>
std::string string_join(R &&vec, std::string_view separator) {
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

// restituisce una copia in minuscolo della stringa di input
inline std::string string_tolower(std::string_view str) {
    auto view = str | std::views::transform(tolower);
    return {view.begin(), view.end()};
}

// restituisce una copia in maiuscolo della stringa di input
inline std::string string_toupper(std::string_view str) {
    auto view = str | std::views::transform(toupper);
    return {view.begin(), view.end()};
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

// converte ogni carattere di spazio in " " e elimina gli spazi ripetuti
inline std::string string_singleline(std::string_view str) {
    std::string ret;
    std::ranges::unique_copy(str | std::views::transform([](auto ch) {
        return isspace(ch) ? ' ' : ch;
    }), std::back_inserter(ret), [](auto a, auto b) {
        return a == ' ' && b == ' ';
    });
    return ret;
}

inline bool parse_num(fixed_point &num, std::string_view str) {
    std::istringstream iss;
    iss.rdbuf()->pubsetbuf(const_cast<char *>(str.begin()), str.size());
    return dec::fromStream(iss, dec::decimal_format(intl::decimal_point(), intl::thousand_sep()), num);
};

template<typename T>
inline T cston(std::string_view str) {
    T ret;
    auto result = std::from_chars(str.begin(), str.end(), ret);
    if (result.ec == std::errc::invalid_argument) {
        throw std::invalid_argument(std::string(str));
    }
    return ret;
}

// converte una stringa in int
inline int cstoi(std::string_view str) {
    return cston<int>(str);
}

#ifdef CHARCONV_FLOAT
float cstof(std::string_view str) {
    return cston<float>(str);
}

double cstod(std::string_view str) {
    return cston<double>(str);
}
#endif

// Cerca la posizione di str2 in str senza fare differenza tra maiuscole e minuscole
inline size_t string_find_icase(std::string_view str, std::string_view str2, size_t index) {
    return std::distance(str.begin(), std::ranges::search(str.substr(index), str2, [](char a, char b) {
        return toupper(a) == toupper(b);
    }).begin());
}

// sostituisce tutte le occorrenze di una stringa in un'altra
void string_replace(std::string &str, std::string_view from, std::string_view to);

// Formatta la stringa data, sostituendo $0 in fmt_args[0], $1 in fmt_args[1] e così via
std::string string_format(std::string_view str, const varargs<std::string_view> &fmt_args);

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
std::string search_regex(const std::string &regex, std::string_view value, int index);

// cerca la regex in str e ritorna tutti i capture del primo valore trovato
std::string search_regex_captures(const std::string &regex, std::string_view value);

// cerca la regex in str e ritorna i valori trovati
std::string search_regex_matches(const std::string &regex, std::string_view value, int index);

// restituisce un'espressione regolare che parsa una riga di una tabella
std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names);

// cerca una data
time_t parse_date(std::string_view value, const std::string &format, const std::string &regex, int index);

// cerca una data e setta il giorno a 1
time_t parse_month(std::string_view value, const std::string &format, const std::string &regex, int index);

// Aggiunge num mesi alla data
inline time_t date_add_month(time_t date, int num) {
    wxDateTime dt(date);
    dt += wxDateSpan(0, num);

    return dt.GetTicks();
}

// Ritorna la data dell'ultimo giorno del mese
inline time_t date_last_day(time_t date) {
    wxDateTime dt(date);
    dt.SetToLastMonthDay(dt.GetMonth(), dt.GetYear());
    return dt.GetTicks();
}

// Ritorna se la data e' compresa nel range indicato
inline bool date_is_between(time_t date, time_t date_begin, time_t date_end) {
    return wxDateTime(date).IsBetween(wxDateTime(date_begin), wxDateTime(date_end));
}

// formatta una data nel formato indicato
inline std::string date_format(time_t date, const std::string &format)  {
    return wxDateTime(date).Format(format).ToStdString();
}

#endif