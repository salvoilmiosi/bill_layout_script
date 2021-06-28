#include "intl.h"

namespace intl {
    static char s_decimal_point = '.';
    static char s_thousand_sep = ',';
    
    char decimal_point() { return s_decimal_point; }
    char thousand_sep() { return s_thousand_sep; }
}

#ifndef USE_WXWIDGETS
#include <locale>

namespace intl {
    bool set_language(const std::string &name) {
        try {
            std::locale loc{name};
            std::locale::global(loc);

            s_decimal_point = std::use_facet<std::numpunct<char>>(loc).decimal_point();
            s_thousand_sep = std::use_facet<std::numpunct<char>>(loc).thousands_sep();

            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}

#else
#include <wx/intl.h>

namespace intl {
    bool set_language(const std::string &name) {
        static wxLocale *current_locale = nullptr;

        wxLanguage lang = wxLANGUAGE_DEFAULT;
        if (!name.empty()) {
            const wxLanguageInfo *lang_info = wxLocale::FindLanguageInfo(name);
            if (lang_info) {
                lang = static_cast<wxLanguage>(lang_info->Language);
            } else {
                return false;
            }
        }

        if (!wxLocale::IsAvailable(lang)) {
            return false;
        }
        
        if (current_locale) delete current_locale;
        current_locale = new wxLocale(lang);

        s_decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
        s_thousand_sep = wxLocale::GetInfo(wxLOCALE_THOUSANDS_SEP, wxLOCALE_CAT_NUMBER).at(0);

        return true;
    }
}

#endif