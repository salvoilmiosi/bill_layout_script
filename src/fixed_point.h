#ifndef __FIXED_POINT_H__
#define __FIXED_POINT_H__

#define DEC_TYPE_LEVEL 0

#include "decimal.h"
#include "utils.h"
#include "exceptions.h"

#include <string>

namespace bls {

    using fixed_point = dec::decimal<10>;

    template<int Prec>
    inline std::string decimal_to_string(dec::decimal<Prec> num) {
        auto str = dec::toString(num);
        auto it = str.rbegin();
        for (; *it == '0' && it != str.rend(); ++it);
        if (*it == '.') ++it;

        str.erase(it.base(), str.end());
        return str;
    }

    template<typename T, int Prec>
    concept convertible_to_decimal = requires(T num) {
        dec::decimal<Prec>(num);
    };

    template<int Prec>
    auto operator <=> (const dec::decimal<Prec> &lhs, const dec::decimal<Prec> &rhs) {
        return lhs.getUnbiased() <=> rhs.getUnbiased();
    }
    template<int Prec>
    auto operator <=> (const dec::decimal<Prec> &lhs, const convertible_to_decimal<Prec> auto &rhs) {
        return lhs <=> dec::decimal<Prec>(rhs);
    }
    template<int Prec>
    auto operator <=> (const convertible_to_decimal<Prec> auto &lhs, const dec::decimal<Prec> &rhs) {
        return dec::decimal<Prec>(lhs) <=> rhs;
    }

}

namespace util {
    template<> inline bls::fixed_point string_to(std::string_view str) {
        bls::fixed_point num;
        isviewstream iss{str};
        if (dec::fromStream(iss, num)) {
            return num;
        } else {
            throw bls::conversion_error(intl::translate("CANT_PARSE_NUMBER", str));
        }
    }
}

#endif