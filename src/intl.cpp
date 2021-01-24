#include "intl.h"

namespace intl {
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

    std::string system_language() {
        return wxLocale::GetLanguageCanonicalName(wxLocale::GetSystemLanguage()).ToStdString();
    }

    static void set_strings() {
        g_decimal_point = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER).at(0);
        g_thousand_sep = wxLocale::GetInfo(wxLOCALE_THOUSANDS_SEP, wxLOCALE_CAT_NUMBER).at(0);

        auto char_to_regex_str = [](char c) -> std::string {
            switch (c) {
            case '.':   return "\\.";
            case '\0':  return "";
            default:    return std::string(&c, 1);
            }
        };

        // I regex di C++11 non supportano i lookbehind, -?\b dovrebbe essere (?<!\S)
        g_number_format = "-?\\b\\d{1,3}(?:"
            + char_to_regex_str(g_thousand_sep) + "\\d{3})*(?:"
            + char_to_regex_str(g_decimal_point) + "\\d+)?\\b";
    }
    
    locale::locale() {
        m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
        set_strings();
    }

    locale::~locale() {
        delete m_locale;
        set_strings();
    }

    void locale::set_language(const std::string &language_name) {
        auto new_lang = wxLocale::FindLanguageInfo(language_name)->Language;
        if (m_locale) {
            if (new_lang == m_locale->GetLanguage()) {
                return;
            }
            delete m_locale;
        }
        m_locale = new wxLocale(new_lang);
        set_strings();
    }

}