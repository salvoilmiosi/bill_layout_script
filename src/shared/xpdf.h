#ifndef __XPDF_H__
#define __XPDF_H__

#include <string>

struct pdf_info {
    std::string filename;
    int num_pages = 0;
    float width;
    float height;
};

enum box_type {
    BOX_RECTANGLE,
    BOX_PAGE,
    BOX_FILE,
    BOX_DISABLED,
};

enum read_mode {
    MODE_RAW,
    MODE_LAYOUT
};

struct pdf_rect {
    float x, y;
    float w, h;
    int page;
    read_mode mode = MODE_RAW;
    box_type type = BOX_RECTANGLE;
};

struct xpdf_error {
    const std::string message;

    xpdf_error(const std::string &message) : message(message) {}
};

std::string pdf_to_text(const pdf_info &info, const pdf_rect &in_rect);

pdf_info pdf_get_info(const std::string &pdf);

#endif // __XPDF_H__