#ifndef __LAYOUT_EXT_H__
#define __LAYOUT_EXT_H__

#include "layout.h"

#include <algorithm>

box_ptr getBoxAt(bill_layout_script &layout, float x, float y, int page);

std::pair<box_ptr, int> getBoxResizeNode(bill_layout_script &layout, float x, float y, int page, float scalex, float scaley);

bill_layout_script copyLayout(const bill_layout_script &layout);

template<typename ... Ts>
static box_ptr &insertAfterSelected(bill_layout_script &layout, Ts && ... args) {
    auto it = std::find_if(layout.m_boxes.begin(), layout.m_boxes.end(), [](const box_ptr &ptr) {
        return ptr->selected;
    });
    if (it != layout.m_boxes.end()) ++it;
    return *layout.m_boxes.emplace(it, std::make_shared<layout_box>(std::forward<Ts>(args) ...));
}

#endif