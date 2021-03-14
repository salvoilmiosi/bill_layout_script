#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <numeric>

#include "functions.h"

constexpr std::string_view RESULT_SEPARATOR = "\x1f";

typedef uint8_t small_int;

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
std::string string_join(R &&vec, std::string_view separator = " ") {
    if (vec.empty()) return "";
    auto it = vec.begin();
    std::string ret(*it);
    for(++it; it != vec.end(); ++it) {
        ret += separator;
        ret += *it;
    }
    return ret;
};

// sostituisce tutte le occorrenze di una stringa in un'altra
void string_replace(std::string &str, std::string_view from, std::string_view to);

// restituisce una copia in minuscolo della stringa di input
std::string string_tolower(std::string_view str);

// restituisce una copia in maiuscolo della stringa di input
std::string string_toupper(std::string_view str);

// elimina gli spazi in eccesso a inizio e fine stringa
std::string string_trim(std::string_view str);

// converte ogni carattere di spazio in " "
std::string string_singleline(std::string_view str);

// legge tutto l'output di uno stream
std::string read_all(std::istream &stream);

// converte una stringa in int
int cstoi(std::string_view str);

// Cerca la posizione di str2 in str senza fare differenza tra maiuscole e minuscole
size_t string_findicase(std::string_view str, std::string_view str2, size_t index);

// Formatta la stringa data, sostituendo $0 in fmt_args[0], $1 in fmt_args[1] e così via
std::string string_format(std::string_view str, const varargs<std::string_view> &fmt_args);

// cerca una data
time_t parse_date(std::string_view value, const std::string &format, const std::string &regex, int index);

// cerca una data e setta il giorno a 1
time_t parse_month(std::string_view value, const std::string &format, const std::string &regex, int index);

// Aggiunge num mesi alla data
time_t date_month_add(time_t month, int num);

// Ritorna la data dell'ultimo giorno del mese
time_t date_last_day(time_t month);

// Ritorna se la data e' compresa nel range indicato
bool date_is_between(time_t date, time_t date_begin, time_t end);

// formatta una data nel formato indicato
std::string date_format(time_t date, const std::string &format);

// cerca la regex in str e ritorna i valori trovati
std::string search_regex_all(const std::string &regex, std::string_view value, int index);

// cerca la regex in str e ritorna tutti i capture del primo valore trovato
std::string search_regex_captures(const std::string &regex, std::string_view value);

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
std::string search_regex(const std::string &regex, std::string_view value, int index);

// rimpiazza in str le occorrenze di format in value
std::string string_replace_regex(const std::string &value, const std::string &regex, const std::string &str);

// restituisce un'espressione regolare che parsa una riga di una tabella
std::string table_row_regex(std::string_view header, const varargs<std::string_view> &names);

// sposta il range indicato alla fine del vettore
template<typename T>
void move_to_end(T &vec, size_t begin, size_t end) {
    for (size_t i=begin; i<end; ++i) {
        vec.push_back(vec[i]);
    }
    vec.erase(vec.begin() + begin, vec.begin() + end);
}

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

#endif