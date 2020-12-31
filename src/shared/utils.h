#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <string>

// divide una stringa per separatore
std::vector<std::string> string_split(const std::string &str, char separator = ',');

// unisce tutte le stringhe in un vettore di stringhe in una
std::string string_join(const std::vector<std::string> &vec, const std::string &separator = " ");

// sostituisce tutte le occorrenze di una stringa in un'altra, restituisce il numero di valori trovati
int string_replace(std::string &str, const std::string &from, const std::string &to);

// restituisce una copia in minuscolo della stringa di input
std::string string_tolower(std::string str);

// restituisce una copia in maiuscolo
std::string string_toupper(std::string str);

// elimina gli spazi in eccesso
void string_trim(std::string &str);

// legge tutta la stringa da uno stream
std::string read_all(std::istream &stream);

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