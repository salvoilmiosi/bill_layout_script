#ifndef __XPDF_H__
#define __XPDF_H__

#include <string>

struct pdf_info {
    int num_pages = 0;
    float width;
    float height;
};

enum read_mode {
    MODE_RAW,
    MODE_SIMPLE,
    MODE_TABLE,
};

struct pdf_rect {
    float x, y;
    float w, h;
    int page;
    read_mode mode = MODE_RAW;
};

struct xpdf_error {
    const std::string message;

    xpdf_error(const std::string &message) : message(message) {}
};

std::string pdf_to_text(const std::string &pdf, const pdf_info &info, const pdf_rect &in_rect);

std::string pdf_whole_file_to_text(const std::string &pdf);

pdf_info pdf_get_info(const std::string &pdf);

#endif // __XPDF_H__