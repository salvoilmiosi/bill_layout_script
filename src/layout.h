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

    struct layout_box : public pdf_rect {
        std::string name;
        std::string script;
        std::string spacers;
        std::string goto_label;
    };

    class layout_box_list : public std::list<layout_box> {
    public:
        using base = std::list<layout_box>;
        bool find_layout_flag = false;
        std::string language;

    public:
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

        friend std::ostream &operator << (std::ostream &output, const layout_box_list &layout);
        friend std::istream &operator >> (std::istream &input, layout_box_list &layout);

        static layout_box_list from_file(const std::filesystem::path &filename) {
            layout_box_list ret;
            std::ifstream ifs(filename);
            if (!ifs) {
                throw file_error(intl::format("CANT_OPEN_FILE", filename.string()));
            }
            ifs >> ret;
            return ret;
        }

        void save_file(const std::filesystem::path &filename) {
            std::ofstream ofs(filename);
            if (!ofs) {
                throw file_error(intl::format("CANT_SAVE_FILE", filename.string()));
            }
            ofs << *this;
        }
    };

}

#endif