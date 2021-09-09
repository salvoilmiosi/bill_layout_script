#ifndef __UNICODE_H__
#define __UNICODE_H__

#include <string>

namespace unicode {

    std::string parseQuotedString(std::string_view str, bool is_regex = false);

    std::string escapeString(std::string_view str);

}

#endif