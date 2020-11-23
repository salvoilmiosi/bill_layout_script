#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <string>
#include <fmt/format.h>

// divide una stringa per separatore
std::vector<std::string> string_split(const std::string &str, char separator = ',');

// unisce tutte le stringhe in un vettore di stringhe in una
std::string string_join(const std::vector<std::string> &vec, const std::string &separator = " ");

// sostituisce tutte le occorrenze di una stringa in un'altra, restituisce il numero di valori trovati
int string_replace(std::string &str, const std::string &from, const std::string &to);

// restituisce una copia in minuscolo della stringa di input
std::string string_tolower(std::string str);

// elimina gli spazi in eccesso
std::string string_trim(std::string str);

// Formatta la stringa data
std::string string_format(std::string str, const std::vector<std::string> &fmt_args);

// converte un numero dal formato italiano al formato universale (trasforma virgole in punti)
std::string parse_number(const std::string &value);

// parsa una stringa dal format json
std::string parse_string(std::string_view value);

// converte i vari formati di data in un formato universale (dd/mm/aaaa)
std::string parse_date(const std::string &format, const std::string &value, int index);

// Aggiunge num mesi alla data
std::string date_month_add(const std::string &month, int num);

// formatta una data nel formato indicato
std::string date_format(const std::string &date, std::string format);

// cerca la regex in str e ritorna i valori trovati
std::vector<std::string> search_regex_all(std::string format, std::string value, int index);

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
std::string search_regex(std::string format, const std::string &value, int index);

// trasforma ogni carattere di spazio in " "
std::string nospace(std::string input);

struct hasher {
    size_t constexpr operator() (char const *input) const {
        return *input ? static_cast<unsigned int>(*input) + 33 * (*this)(input + 1) : 5381;
    }
    
    size_t operator() (const std::string& str) const {
        return (*this)(str.c_str());
    }
};

// restituisce l'hash di una stringa
template<typename T> size_t constexpr hash(T&& t) {
    return hasher{}(std::forward<T>(t));
}

#endif