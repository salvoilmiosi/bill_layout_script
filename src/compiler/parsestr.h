#ifndef __PARSE_STR_H__
#define __PARSE_STR_H__

#include <string>

// parsa una stringa
bool parse_string(std::string &out, std::string_view value);

// parsa una stringa regexp
bool parse_string_regexp(std::string &out, std::string_view value);

#endif