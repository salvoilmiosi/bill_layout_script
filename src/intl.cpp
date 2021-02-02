#include "intl.h"

#include <fmt/format.h>
#include <stdexcept>

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
            case '.':
            case '+':
            case '*':
            case '?':
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '|':
            case '\\':
                return std::string("\\") + c;
            case '\0':  return "";
            default:    return std::string(&c, 1);
            }
        };

        g_number_format = "-?\\d{1,3}(?:"
            + char_to_regex_str(g_thousand_sep) + "\\d{3})*(?:"
            + char_to_regex_str(g_decimal_point) + "\\d+)?";
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
        auto info = wxLocale::FindLanguageInfo(language_name);
        if (info) {
            int new_lang = info->Language;
            if (m_locale) {
                if (new_lang == m_locale->GetLanguage()) {
                    return;
                }
                delete m_locale;
            }
            m_locale = new wxLocale(new_lang);
            set_strings();
        } else {
            throw std::runtime_error(fmt::format("Condice lingua invalido: {}", language_name));
        }
    }

}