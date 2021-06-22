#include "intl.h"
#include <iostream>

namespace intl {
    static wxLocale *s_current_locale = nullptr;

    static char s_decimal_point;
    static char s_thousand_sep;

    wxLanguage string_to_language(const std::string &str) {
        const wxLanguageInfo *lang_info = wxLocale::FindLanguageInfo(str);
        if (lang_info) {
            return static_cast<wxLanguage>(lang_info->Language);
        } else {
            return wxLANGUAGE_UNKNOWN;
        }
    }

    bool set_language(wxLanguage lang) {
        if (!wxLocale::IsAvailable(lang)) {
            std::cerr << "Lingua non supportata: " << wxLocale::GetLanguageName(lang) << std::endl;
            return false;
        }

        if (s_current_locale) delete s_current_locale;

        s_current_locale = new wxLocale(lang);

        s_decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
        s_thousand_sep = wxLocale::GetInfo(wxLOCALE_THOUSANDS_SEP, wxLOCALE_CAT_NUMBER).at(0);
        return true;
    }

    bool set_language_from_string(const std::string &str) {
        wxLanguage lang = intl::string_to_language(str);
        if (lang != wxLANGUAGE_UNKNOWN) {
            return set_language(lang);
        }
        return false;
    }

    void set_default_language() {
        set_language(wxLANGUAGE_DEFAULT);
    }

    void reset_language() {
        if (s_current_locale) delete s_current_locale;
        s_current_locale = nullptr;
    }

    char decimal_point() { return s_decimal_point; }
    char thousand_sep() { return s_thousand_sep; }
}