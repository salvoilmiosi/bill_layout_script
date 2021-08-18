#ifndef __FIXED_POINT_H__
#define __FIXED_POINT_H__

#define DEC_TYPE_LEVEL 0
#define DEC_ALLOW_SPACESHIP_OPER 1

#include "decimal.h"
#include "utils.h"

#include <string>

namespace bls {

    using fixed_point = dec::decimal<10>;

    inline std::string fixed_point_to_string(fixed_point num) {
        auto str = dec::toString(num);
        auto it = str.rbegin();
        for (; *it == '0' && it != str.rend(); ++it);
        if (*it == '.') ++it;

        str.erase(it.base(), str.end());
        return str;
    }

}

namespace util {
    template<> inline bls::fixed_point string_to(std::string_view str) {
        bls::fixed_point num;
        isviewstream iss{str};
        if (dec::fromStream(iss, num)) {
            return num;
        } else {
            throw std::invalid_argument(intl::format("COULD_NOT_CONVERT_TO_NUMBER", str));
        }
    }
}

#endif