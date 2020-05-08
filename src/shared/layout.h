#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>

struct layout_box : public xpdf::rect {
    std::string name = "";
    bool selected = false;
};

class layout_bolletta {
public:
    layout_bolletta();
    std::vector<layout_box>::iterator getBoxUnder(float x, float y, int page);

public:
    std::vector<layout_box> boxes;
};

#endif