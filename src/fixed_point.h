#ifndef __FIXED_POINT_H__
#define __FIXED_POINT_H__

#define DEC_TYPE_LEVEL 0
#define DEC_ALLOW_SPACESHIP_OPER 1

#include "decimal.h"
#include <string>
#include <ranges>

using fixed_point = dec::decimal<10>;

inline std::string fixed_point_to_string(fixed_point num) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << num;

    std::string str = ss.str();
    auto it = str.rbegin();
    for (; *it == '0' && it != str.rend(); ++it);
    if (*it == '.') ++it;

    str.erase(it.base(), str.end());
    return str;
}

#endif