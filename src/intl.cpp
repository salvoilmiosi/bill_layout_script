#include "intl.h"

#include <boost/locale.hpp>

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
            auto custom_lbm = boost::locale::localization_backend_manager::global();
            #ifdef _WIN32
                custom_lbm.select("winapi");
            #else
                custom_lbm.select("posix");
            #endif

            auto numloc = boost::locale::generator{custom_lbm}(name);
            auto &numfacet = std::use_facet<std::numpunct<char>>(numloc);
            s_thousand_sep = numfacet.thousands_sep();
            s_decimal_point = numfacet.decimal_point();
            
            std::locale loc = boost::locale::generator{}(name);
            s_encoding = std::use_facet<boost::locale::info>(loc).encoding();
            std::locale::global(loc);

            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}