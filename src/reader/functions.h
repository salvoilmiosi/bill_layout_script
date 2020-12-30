#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <vector>
#include <string>

// Converte la stringa data in un numero dato il locale
std::string parse_number(const std::string &str);

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
std::string nonewline(std::string input);

#endif
