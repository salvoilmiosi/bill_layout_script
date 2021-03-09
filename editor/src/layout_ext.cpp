#include "layout_ext.h"

constexpr float RESIZE_TOLERANCE = 8.f;

box_iterator getBoxAt(bill_layout_script &layout, float x, float y, int page) {
    auto check_box = [&](const layout_box &box) {
        return (x > box.x && x < box.x + box.w && y > box.y && y < box.y + box.h && box.page == page);
    };
    auto it = std::ranges::find_if(layout.m_boxes, [&](const auto &box) {
        return box.selected && check_box(box);
    });
    if (it != layout.m_boxes.end()) return it;
    return std::ranges::find_if(layout.m_boxes, [&](const auto &box) {
        return check_box(box);
    });
}

std::pair<box_iterator, int> getBoxResizeNode(bill_layout_script &layout, float x, float y, int page, float scalex, float scaley) {
    float nw = RESIZE_TOLERANCE / scalex;
    float nh = RESIZE_TOLERANCE / scaley;
    auto check_box = [&](const layout_box &box) {
        if (box.page == page) {
            int node = 0;
            if (y > box.y - nh && y < box.y + box.h + nh) {
                if (x > box.x - nw && x < box.x + nw) {
                    node |= RESIZE_LEFT;
                } else if (x > box.x + box.w - nw && x < box.x + box.w + nw) {
                    node |= RESIZE_RIGHT;
                }
            }
            if (x > box.x - nw && x < box.x + box.w + nw) {
                if (y > box.y - nh && y < box.y + nh) {
                    node |= RESIZE_TOP;
                } else if (y > box.y + box.h - nh && y < box.y + box.h + nh) {
                    node |= RESIZE_BOTTOM;
                }
            }
            return node;
        }
        return 0;
    };
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        if (it->selected) {
            int node = check_box(*it);
            if (node) return std::make_pair(it, node);
        }
    }
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        int node = check_box(*it);
        if (node) return std::make_pair(it, node);
    }
    return std::make_pair(layout.m_boxes.end(), 0);
}