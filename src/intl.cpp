#include "intl.h"

#include <boost/locale.hpp>
#include <sstream>

namespace intl {
    static char s_decimal_point = '.';
    static char s_thousand_sep = ',';

    static std::string s_encoding;
    
    char decimal_point() { return s_decimal_point; }
    char thousand_sep() { return s_thousand_sep; }

    std::string to_utf8(std::string_view str) {
        return boost::locale::conv::to_utf<char>(
            str.data(), str.data() + str.size(), s_encoding);
    }

    std::string from_utf8(std::string_view str) {
        return boost::locale::conv::from_utf<char>(
            str.data(), str.data() + str.size(), s_encoding);
    }

    bool set_language(const std::string &name) {
        try {
            std::locale loc = boost::locale::generator{}(name);
            std::locale::global(loc);

            std::stringstream ss;
            ss << boost::locale::as::number << 1000.5;
            std::string num_str = ss.str();

            s_thousand_sep = num_str[1];
            if (s_thousand_sep == '0') {
                s_thousand_sep='\0';
                s_decimal_point = num_str[4];
            } else {
                s_decimal_point = num_str[5];
            }

            s_encoding = std::use_facet<boost::locale::info>(loc).encoding();

            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}