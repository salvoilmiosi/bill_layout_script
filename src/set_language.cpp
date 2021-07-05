#include "set_language.h"

#include <boost/locale.hpp>

namespace bls {
    bool set_language(const std::string &name) {
        try {
            boost::locale::generator gen;
            std::locale::global(gen(name.empty() ? "" : name + ".UTF-8"));
            return true;
        } catch (std::runtime_error) {
            return false;
        }
    }
}