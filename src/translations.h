#ifndef __TRANSLATIONS_H__
#define __TRANSLATIONS_H__

#include <boost/locale.hpp>
#include <fmt/format.h>

namespace intl {
    static const std::locale &get_transl_locale() {
        static const struct translation_locale {
            std::locale loc;
            translation_locale() {
                boost::locale::generator gen;
                gen.categories(boost::locale::message_facet);
                gen.add_messages_path(".");
                gen.add_messages_domain("bls");
                loc = gen("");
            }
        } loc;
        return loc.loc;
    }

    template<typename ... Ts>
    std::string format(const char *fmt_str, Ts &&...args) {
        try {
            return fmt::vformat(boost::locale::gettext(fmt_str, get_transl_locale()),
                fmt::make_format_args(std::forward<Ts>(args) ...));
        } catch (...) {
            return fmt_str;
        }
    }
}

#endif