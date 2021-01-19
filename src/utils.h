#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <string>
#include <sstream>

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

template<typename T> inline T cread_simple(const std::string &str) {
    std::istringstream iss(str);
    iss.imbue(std::locale("C"));
    T val;
    iss >> val;
    return val;
}

inline int   cstoi(const std::string &str) { return cread_simple<int>(str); }
inline float cstof(const std::string &str) { return cread_simple<float>(str); }

// sposta il range indicato alla fine del vettore
template<typename T>
void vector_move_to_end(std::vector<T> &vec, size_t begin, size_t end) {
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