#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>

enum box_type {
    BOX_STRING,
    BOX_NUMBER,
    BOX_NUMBER_ARRAY,
    BOX_NUMBER_GRID,
};

struct layout_box : public xpdf::rect {
    bool selected = false;
    std::string name = "";
    std::string parse_string = "";
    box_type type = BOX_STRING;
};

class layout_bolletta {
public:
    layout_bolletta();
    std::vector<layout_box>::iterator getBoxUnder(float x, float y, int page);

public:
    std::vector<layout_box> boxes;
};

#endif