#ifndef __FIXED_POINT_H__
#define __FIXED_POINT_H__

#include "decimal.h"
#include <string>

using fixed_point = dec::decimal<10>;

inline std::string fixed_point_to_string(fixed_point num) {
    std::string str = dec::toString(num);
    auto it = str.rbegin();
    for (; *it == '0' && it != str.rend(); ++it);
    if (*it == '.') ++it;

    str.erase(it.base(), str.end());
    return str;
}

#endif