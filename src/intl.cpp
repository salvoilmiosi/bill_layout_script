#include "intl.h"

#ifdef USE_BOOST_LOCALE
#include <boost/locale.hpp>
#include <sstream>
#else
#include <locale>
#endif

namespace intl {
    static char s_decimal_point = '.';
    static char s_thousand_sep = ',';
    
    char decimal_point() { return s_decimal_point; }
    char thousand_sep() { return s_thousand_sep; }

    bool set_language(const std::string &name) {
        try {
#ifdef USE_BOOST_LOCALE
            std::locale::global(boost::locale::generator{}(name));

            std::stringstream ss;
            ss << boost::locale::as::number << 1000.5;
            std::string num_str = ss.str();

            s_thousand_sep = num_str[1];
            if (s_thousand_sep >= '0' && s_thousand_sep <='9') {
                s_thousand_sep='\0';
                s_decimal_point = num_str[4];
            } else {
                s_decimal_point = num_str[5];
            }
#else
            std::locale loc{name};
            std::locale::global(loc);

            const auto &facet = std::use_facet<std::numpunct<char>>(loc);

            s_decimal_point = facet.decimal_point();
            s_thousand_sep = facet.thousands_sep();
#endif
            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}