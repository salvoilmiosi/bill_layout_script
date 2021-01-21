#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"

#include <memory>
#include <vector>
#include <cstring>
#include <filesystem>
#include <fstream>

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
    layout_error(const std::string &message) : std::runtime_error(message) {}
};

using box_ptr = std::shared_ptr<layout_box>;

std::ostream &operator << (std::ostream &out, const class bill_layout_script &obj);
std::istream &operator >> (std::istream &in, class bill_layout_script &obj);

struct bill_layout_script : std::vector<box_ptr> {
    static bill_layout_script from_stream(std::istream &in) {
        bill_layout_script ret;
        if (!(in >> ret)) {
            throw layout_error("Impossibile aprire questo layout");
        }
        return ret;
    }

    static bill_layout_script from_file(const std::filesystem::path &filename) {
        std::ifstream ifs(filename);
        return from_stream(ifs);
    }

    bool save_file(const std::filesystem::path &filename) {
        std::ofstream ofs(filename);
        return ! (ofs << *this).fail();
    }
};

#endif