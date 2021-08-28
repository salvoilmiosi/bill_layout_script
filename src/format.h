#ifndef __FORMAT_H__
#define __FORMAT_H__

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
    inline std::string format(fmt::string_view fmt_str, Ts && ... args) {
        return fmt::vformat(fmt_str, fmt::make_format_args(args ... ));
    }

    using format_error = fmt::format_error;
}
#else
#include <format>
#endif

#endif