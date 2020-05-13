#ifndef __UTILS_H__
#define __UTILS_H__

#include <vector>
#include <string>
#include <sstream>

// divide una stringa nelle sue righe
std::vector<std::string> read_lines(const std::string &str);

// divide una stringa con separatore spazio
std::vector<std::string> tokenize(const std::string &str);

// unisce tutte le stringhe in un vettore di stringhe in una
std::string implode(const std::vector<std::string> &vec, const std::string &separator = " ");

// sostituisce tutte le occorrenze di una stringa in un'altra, restituisce il numero di valori trovati
int string_replace(std::string &str, const std::string &from, const std::string &to);

// converte un numero dal formato italiano al formato universale (trasforma virgole in punti)
std::string parse_number(const std::string &value);

// converte i vari formati di data in un formato universale (dd/mm/aaaa)
std::string parse_date(std::string format, const std::string &value);

// cerca $NUMBER in str e ritorna il valore trovato
std::string search_regex(std::string format, const std::string &value);

#endif