#ifndef __PARSE_STR_H__
#define __PARSE_STR_H__

#include <string>

// parsa una stringa
std::string parse_string(std::string_view value);

// parsa una stringa regexp
std::string parse_string_regexp(std::string_view value);

#endif