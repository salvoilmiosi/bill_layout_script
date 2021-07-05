#ifndef __INTL_H__
#define __INTL_H__

#include <string>

namespace intl {
    bool set_language(const std::string &name);
    
    char decimal_point();
    char thousands_sep();
    std::string grouping();

    std::string to_utf8(std::string_view str);
    std::string from_utf8(std::string_view str);
};

#endif