#ifndef __INTL_H__
#define __INTL_H__

#include <string>

namespace intl {
    typedef int language;

    char decimal_point();

    char thousand_sep();

    const std::string &number_format();

    std::string language_string(language lang);

    std::string language_name(language lang);
    
    language language_code(const std::string &lang);

    language system_language();

    bool valid_language(language lang);

    void set_language(language lang);
    
    void reset_language();
}

#endif