#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"
#include "intl.h"

#include <list>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fmt/format.h>

#define RESIZE_TOP      1 << 0
#define RESIZE_BOTTOM   1 << 1
#define RESIZE_LEFT     1 << 2
#define RESIZE_RIGHT    1 << 4

struct layout_box : public pdf_rect {
    bool selected = false;
    std::string name;
    std::string script;
    std::string spacers;
    std::string goto_label;
};

struct layout_error : std::runtime_error {
    layout_error(auto &&message) : std::runtime_error(std::forward<decltype(message)>(message)) {}
};

std::ostream &operator << (std::ostream &out, const class bill_layout_script &obj);
std::istream &operator >> (std::istream &in, class bill_layout_script &obj);

class bill_layout_script {
public:
    std::list<layout_box> m_boxes;
    intl::language language_code{};

public:
    bill_layout_script() = default;

    void clear() {
        m_boxes.clear();
        language_code = {};
    }

    static bill_layout_script from_file(const std::filesystem::path &filename) {
        bill_layout_script ret;
        std::ifstream ifs(filename);
        if (!ifs) {
            throw layout_error(fmt::format("Impossibile aprire il file {}", filename.string()));
        }
        ifs >> ret;
        return ret;
    }

    void save_file(const std::filesystem::path &filename) {
        std::ofstream ofs(filename);
        if (!ofs) {
            throw layout_error(fmt::format("Impossibile salvare il file {}", filename.string()));
        }
        ofs << *this;
    }
};

#endif