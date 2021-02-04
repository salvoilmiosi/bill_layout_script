#ifndef __INTL_H__
#define __INTL_H__

#include <string>
#include <wx/intl.h>

namespace intl {
    char decimal_point();
    char thousand_sep();

    const std::string &number_format();

    std::string language_string(int lang);
    std::string language_name(int lang);
    
    int language_int(const std::string &lang);

    int system_language();

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