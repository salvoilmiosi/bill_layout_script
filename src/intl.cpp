#include "intl.h"

#include <wx/intl.h>
#include <fmt/format.h>
#include <stdexcept>

namespace intl {
    static char g_decimal_point;
    static char g_thousand_sep;
    static std::string g_number_format;

    static wxLocale *g_locale = nullptr;

    char decimal_point() {
        return g_decimal_point;
    }

    char thousand_sep() {
        return g_thousand_sep;
    }

    const std::string &number_format() {
        return g_number_format;
    }

    std::string language_string(language lang) {
        return wxLocale::GetLanguageCanonicalName(lang).ToStdString();
    }

    std::string language_name(language lang) {
        return wxLocale::GetLanguageName(lang).ToStdString();
    }

    language language_code(const std::string &lang) {
        auto info = wxLocale::FindLanguageInfo(lang);
        if (info) {
            return info->Language;
        } else {
            throw std::runtime_error(fmt::format("Condice lingua invalido: {}", lang));
        }
    }

    language system_language() {
        return wxLocale::GetSystemLanguage();
    }

    bool valid_language(language lang) {
        return lang != 0;
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

        g_number_format = "-?(?:\\d{1,3}(?:"
            + char_to_regex_str(g_thousand_sep) + "\\d{3})*(?:"
            + char_to_regex_str(g_decimal_point) + "\\d+)?|\\d+)(?!\\d)";
    }

    void set_language(language lang) {
        if (wxLocale::IsAvailable(lang)) {
            if (g_locale) {
                if (lang == g_locale->GetLanguage()) {
                    return;
                }
                delete g_locale;
            }
            g_locale = new wxLocale(lang);
            set_strings();
        } else {
            throw std::runtime_error(fmt::format("Lingua non supportata: {}", language_name(lang)));
        }
    }

    void reset_language() {
        if (g_locale) {
            delete g_locale;
            g_locale = nullptr;
        }
        set_strings();
    }

}