#ifndef __INTL_H__
#define __INTL_H__

#include <string>
#include <wx/intl.h>

namespace intl {
    char decimal_point();
    char thousand_sep();

    const std::string &number_format();

    std::string language_string(int lang);
    int language_int(const std::string &lang);

    class locale {
    public:
        locale();
        ~locale();

        void set_language(int lang);
        
    private:
        wxLocale *m_locale = nullptr;
    };
}

#endif