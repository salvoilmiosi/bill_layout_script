#ifndef __LAYOUT_EXT_H__
#define __LAYOUT_EXT_H__

#include "layout.h"

#include <algorithm>

using box_iterator = std::list<layout_box>::iterator;

box_iterator getBoxAt(bill_layout_script &layout, float x, float y, int page);

std::pair<box_iterator, int> getBoxResizeNode(bill_layout_script &layout, float x, float y, int page, float scalex, float scaley);

template<typename ... Ts>
static layout_box &insertAfterSelected(bill_layout_script &layout, Ts && ... args) {
    auto it = std::ranges::find_if(layout.m_boxes, [](const layout_box &ptr) {
        return ptr.selected;
    });
    if (it != layout.m_boxes.end()) ++it;
    return *layout.m_boxes.emplace(it, std::forward<Ts>(args) ...);
}

#endif