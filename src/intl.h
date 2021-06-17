#ifndef __INTL_H__
#define __INTL_H__

#include <wx/intl.h>

namespace intl {
    wxLanguage string_to_language(const std::string &str);

    bool set_language(wxLanguage lang);
    void reset_language();
    
    char decimal_point();
    char thousand_sep();
};

#endif