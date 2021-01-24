#ifndef __INTL_H__
#define __ITNL_H__

#include <string>

namespace intl {
    char decimal_point();
    char thousand_sep();

    const std::string &number_format();

    void init();
    void cleanup();

    void change_language(const std::string &language_name);
}

#endif