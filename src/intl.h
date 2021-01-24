#ifndef __INTL_H__
#define __INTL_H__

#include <string>
#include <wx/intl.h>

namespace intl {
    char decimal_point();
    char thousand_sep();

    const std::string &number_format();

    std::string system_language();

    class locale {
    public:
        locale();
        ~locale();

        void set_language(const std::string &language_name);
        
    private:
        wxLocale *m_locale = nullptr;
    };
}

#endif