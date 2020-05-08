#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

struct box_layout : public xpdf::rect {
    std::string name = "";
    bool selected = false;
};

#endif