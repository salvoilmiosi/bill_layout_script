#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <vector>
#include <string>

// Converte la stringa data in un numero dato il locale
std::string parse_number(const std::string &str);

// Formatta la stringa data
std::string string_format(std::string str, const std::vector<std::string> &fmt_args);

// converte i vari formati di data in un formato universale (dd/mm/aaaa)
std::string parse_date(const std::string &format, const std::string &value, int index);

// Aggiunge num mesi alla data
std::string date_month_add(const std::string &month, int num);

// formatta una data nel formato indicato
std::string date_format(const std::string &date, std::string format);

// cerca la regex in str e ritorna i valori trvati
std::vector<std::string> search_regex_all(const std::string &format, const std::string &value, int index);

// rimpiazza in str le occorrenze di format in value
std::string string_replace_regex(const std::string &format, const std::string &value, const std::string &str);

// cerca la regex in str e ritorna il primo valore trovato, oppure stringa vuota
std::string search_regex(const std::string &format, const std::string &value, int index);

// trasforma ogni carattere di spazio in " "
std::string nonewline(std::string input);

#endif
