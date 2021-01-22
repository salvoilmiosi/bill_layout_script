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

// converte una stringa in int
int cstoi(std::string_view str, int base = 10);

// converte una stringa in float 
double cstof(std::string_view str, int base = 10);

// Converte la stringa data in un numero dato il locale
std::string parse_number(const std::string &str);

// Cerca la posizione di str2 in str senza fare differenza tra maiuscole-minuscole
size_t string_findicase(std::string_view str, std::string_view str2, size_t index);

// Formatta la stringa data
std::string string_format(std::string str, const std::vector<std::string> &fmt_args);

// converte i vari formati di data in formato yyyy-mm-dd
std::string parse_date(const std::string &format, std::string_view value, const std::string &regex, int index);

// converte i vari formati di data in formato yyyy-mm
std::string parse_month(const std::string &format, std::string_view value, const std::string &regex, int index);

// Aggiunge num mesi alla data
std::string date_month_add(std::string_view month, int num);

// formatta una data nel formato indicato
std::string date_format(std::string_view date, const std::string &format);

// cerca la regex in str e ritorna i valori trvati
std::vector<std::string> search_regex_all(const std::string &format, std::string_view value, int index);

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
std::string search_regex(const std::string &format, std::string_view value, int index);

// rimpiazza in str le occorrenze di format in value
std::string &string_replace_regex(std::string &value, const std::string &format, const std::string &str);

// trasforma ogni carattere di spazio in " "
std::string singleline(std::string input);

// parsa una stringa (elimina le virgolette a inizio e fine e legge gli escape)
bool parse_string(std::string &out, std::string_view value);

// parsa una stringa regexp
bool parse_string_regexp(std::string &out, std::string_view value);

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