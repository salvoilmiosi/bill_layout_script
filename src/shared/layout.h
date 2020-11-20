#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "pdf_document.h"

#include <vector>
#include <cstring>

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

struct layout_error {
    std::string message;

    layout_error(const std::string &message) : message(message) {}
};

using bill_layout_script = std::vector<layout_box>;

std::ostream &operator << (std::ostream &out, const bill_layout_script &obj);
std::istream &operator >> (std::istream &in, bill_layout_script &obj);

#endif