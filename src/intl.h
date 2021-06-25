#ifndef __INTL_H__
#define __INTL_H__

#include <string>

namespace intl {
    bool set_language(const std::string &name);
    
    char decimal_point();
    char thousand_sep();
};

#endif