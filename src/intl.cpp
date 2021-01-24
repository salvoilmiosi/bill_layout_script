#include "intl.h"

#include <wx/intl.h>

namespace intl {
    static wxLocale *g_locale = nullptr;

    static char g_decimal_point;
    static char g_thousand_sep;
    static std::string g_number_format;

    char decimal_point() {
        return g_decimal_point;
    }

    char thousand_sep() {
        return g_thousand_sep;
    }

    const std::string &number_format() {
        return g_number_format;
    }

    static void set_strings() {
        g_decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
        g_thousand_sep = wxLocale::GetInfo(wxLOCALE_THOUSANDS_SEP, wxLOCALE_CAT_NUMBER).at(0);

        // I regex di C++11 non supportano i lookbehind, -?\b dovrebbe essere (?<!\S)
        g_number_format = "-?\\b\\d{1,3}(?:"
            + (g_thousand_sep == '.' ? "\\." : std::string(&g_thousand_sep, 1)) + "\\d{3})*(?:"
            + (g_decimal_point == '.' ? "\\." : std::string(&g_decimal_point, 1)) + "\\d+)?\\b";
    }

    void init() {
        if (!g_locale) {
            g_locale = new wxLocale;
            set_strings();
        }
    }

    void cleanup() {
        if (g_locale) {
            delete g_locale;
            g_locale = nullptr;
            set_strings();
        }
    }

    void change_language(const std::string &language_name) {
        if (g_locale) {
            delete g_locale;
        }
        g_locale = new wxLocale(wxLocale::FindLanguageInfo(language_name)->Language);
        set_strings();
    }
}