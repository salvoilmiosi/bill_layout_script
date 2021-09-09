#include "translations.h"

extern const char __translation_bls_it[];
extern const int __translation_bls_it_length;

extern const char __translation_bls_en[];
extern const int __translation_bls_en_length;

namespace intl {
    static const std::locale s_messages_locale = [] {
        namespace blg = boost::locale::gnu_gettext;

        blg::messages_info info;
        info.paths.push_back("");
        info.domains.push_back(blg::messages_info::domain("bls"));

        boost::locale::generator gen;
        gen.categories(boost::locale::information_facet);
        std::locale loc = gen("");

        const auto &properties = std::use_facet<boost::locale::info>(loc);
        info.language = properties.language();
        info.country = properties.country();
        info.variant = properties.variant();
        info.encoding = "UTF-8";
        info.callback = [](const std::string &filename, const std::string &encoding) -> std::vector<char> {
            if (filename.starts_with("/it_IT/")) {
                return {__translation_bls_it, __translation_bls_it + __translation_bls_it_length};
            } else {
                return {__translation_bls_en, __translation_bls_en + __translation_bls_en_length};
            }
        };

        return std::locale(loc, blg::create_messages_facet<char>(info));
    }();
    
    const std::locale &messages_locale() {
        return s_messages_locale;
    }
}