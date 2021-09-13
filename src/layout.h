#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"

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

        std::filesystem::path filename;

        layout_box_list() = default;
        explicit layout_box_list(const std::filesystem::path &filename);

        void save_file();

        void clear() {
            base::clear();
            find_layout_flag = false;
            language.clear();
            filename.clear();
        }
    };

}

#endif