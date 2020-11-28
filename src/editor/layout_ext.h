#ifndef __LAYOUT_EXT_H__
#define __LAYOUT_EXT__

#include "layout.h"

box_ptr getBoxAt(bill_layout_script &boxes, float x, float y, int page);

std::pair<box_ptr, int> getBoxResizeNode(bill_layout_script &boxes, float x, float y, int page, float scalex, float scaley);

bill_layout_script copyLayout(const bill_layout_script &layout);

#endif