#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"
#include "exceptions.h"
#include "utils.h"

#include <list>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace bls {

    DEFINE_ENUM_FLAGS_IN_NS(bls, box_flags,
        (DISABLED)
        (PAGE)
        (NOREAD)
        (SPACER)
        (TRIM)
    )

    struct layout_box : public pdf_rect {
        std::string name;
        std::string script;
        std::string spacers;
        std::string goto_label;
        enums::bitset<box_flags> flags;
    };

    struct layout_box_list : std::list<layout_box> {
        using base = std::list<layout_box>;
        bool find_layout_flag = false;
        std::string language;

        layout_box_list() = default;

        void clear() {
            base::clear();
            find_layout_flag = false;
            language.clear();
        }

        auto get_box_iterator(const layout_box *ptr) {
            return std::ranges::find_if(*this, [&](const auto &box) {
                return &box == ptr;
            });
        };
        
        template<typename ... Ts>
        auto insert_after(base::iterator pos, Ts && ... args) {
            if (pos != base::end()) ++pos;
            return base::emplace(pos, std::forward<Ts>(args) ...);
        }

        static layout_box_list from_file(const std::filesystem::path &filename);

        void save_file(const std::filesystem::path &filename);
    };

}

#endif