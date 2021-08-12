#ifndef __TRANSLATIONS_H__
#define __TRANSLATIONS_H__

#include <boost/locale.hpp>

#ifdef USE_FMTLIB
#include <fmt/format.h>
namespace std {
    inline std::string vformat(fmt::string_view fmt_str, fmt::format_args args) {
        return fmt::vformat(fmt_str, args);
    }

    template<typename ... Ts>
    inline auto make_format_args(Ts && ... args) {
        return fmt::make_format_args(std::forward<Ts>(args) ... );
    }

    template<typename ... Ts>
    inline std::string format(fmt::format_string<Ts...> fmt_str, Ts && ... args) {
        return fmt::vformat(fmt_str, fmt::make_format_args(args ...));
    }

    using format_error = fmt::format_error;
}
#else
#include <format>
#endif

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
    std::string format(const char *fmt_str, Ts && ... args) {
        try {
            return std::vformat(boost::locale::gettext(fmt_str, get_transl_locale()),
                std::make_format_args(std::forward<Ts>(args) ...));
        } catch (std::format_error) {
            return fmt_str;
        }
    }

    template<typename ... Ts>
    std::string format(const std::string &fmt_str, Ts && ... args) {
        return format(fmt_str.c_str(), std::forward<Ts>(args) ...);
    }
}

#endif