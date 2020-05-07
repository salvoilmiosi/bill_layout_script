#ifndef __XPDF_H__
#define __XPDF_H__

#include <wx/wx.h>
#include <string>

#include "pipe.h"

namespace xpdf {

struct pdf_info {
    int num_pages;
    float width;
    float height;
};

struct rect {
    float x, y;
    float w, h;
    int page;
};

std::string pdf_to_text(const std::string &app_dir, const std::string &pdf, const pdf_info &info, const rect &in_rect);

pdf_info pdf_get_info(const std::string &app_dir, const std::string &pdf);

wxImage pdf_to_image(const std::string &app_dir, const std::string &pdf, int page);

}

#endif // __XPDF_H__