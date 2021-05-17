#ifndef __INTL_H__
#define __INTL_H__

#include <wx/intl.h>

namespace intl {
    bool set_language(wxLanguage lang);
    void reset_language();
    
    char decimal_point();
    char thousand_sep();
};

#endif