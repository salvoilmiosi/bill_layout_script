#ifndef __TRANSLATIONS_H__
#define __TRANSLATIONS_H__

#include <boost/locale.hpp>

#include "format.h"
#include "enums.h"

namespace intl {
    const std::locale &messages_locale();

    template<typename ... Ts>
    std::string translate(const char *fmt_str, Ts && ... args) {
        try {
            return std::vformat(boost::locale::gettext(fmt_str, messages_locale()),
                std::make_format_args(std::forward<Ts>(args) ...));
        } catch (std::format_error) {
            return fmt_str;
        }
    }

    template<typename ... Ts>
    std::string translate(const std::string &fmt_str, Ts && ... args) {
        return translate(fmt_str.c_str(), std::forward<Ts>(args) ...);
    }

    template<enums::reflected_enum E>
    std::string enum_label(E value) {
        return translate(std::format("{}::{}", enums::enum_name_v<E>, enums::to_string(value)));
    }
}

#endif