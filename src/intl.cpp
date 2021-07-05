#include "intl.h"

#include <boost/locale.hpp>

namespace intl {
    static char s_decimal_point = '.';
    static char s_thousands_sep = ',';
    static std::string s_grouping;
    static std::string s_encoding;
    
    char decimal_point() { return s_decimal_point; }
    char thousands_sep() { return s_thousands_sep; }
    std::string grouping() { return s_grouping; }

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

            auto &numfacet = std::use_facet<std::numpunct<char>>(loc);
            s_thousands_sep = numfacet.thousands_sep();
            s_decimal_point = numfacet.decimal_point();
            s_grouping = numfacet.grouping();
            
            s_encoding = std::use_facet<boost::locale::info>(loc).encoding();
        
            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}