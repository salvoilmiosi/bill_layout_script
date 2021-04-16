#include "intl.h"

namespace intl {
    static char s_decimal_point;
    static char s_thousand_sep;

    locale::locale(wxLanguage lang) : wxLocale(lang) {
        s_decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
        s_thousand_sep = wxLocale::GetInfo(wxLOCALE_THOUSANDS_SEP, wxLOCALE_CAT_NUMBER).at(0);
    }

    char decimal_point() {
        return s_decimal_point;
    }

    char thousand_sep() {
        return s_thousand_sep;
    }
}