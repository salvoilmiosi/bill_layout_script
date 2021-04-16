#ifndef __INTL_H__
#define __INTL_H__

#include <wx/intl.h>

namespace intl {
    struct locale : wxLocale {
        locale(wxLanguage lang);
    };

    char decimal_point();
    char thousand_sep();
};

#endif