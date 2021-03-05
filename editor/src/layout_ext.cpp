#include "layout_ext.h"

constexpr float RESIZE_TOLERANCE = 8.f;

box_ptr getBoxAt(bill_layout_script &layout, float x, float y, int page) {
    auto check_box = [&](const layout_box &box) {
        return (x > box.x && x < box.x + box.w && y > box.y && y < box.y + box.h && box.page == page);
    };
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        if ((*it)->selected && check_box(**it)) return *it;
    }
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        if (check_box(**it)) return *it;
    }
    return nullptr;
}

std::pair<box_ptr, int> getBoxResizeNode(bill_layout_script &layout, float x, float y, int page, float scalex, float scaley) {
    float nw = RESIZE_TOLERANCE / scalex;
    float nh = RESIZE_TOLERANCE / scaley;
    auto check_box = [&](box_ptr &it) -> std::pair<box_ptr, int> {
        layout_box &box = *it;
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
            return std::make_pair(it, node);
        }
        return std::make_pair(nullptr, 0);
    };
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        if ((*it)->selected) {
            auto res = check_box(*it);
            if (res.second) return res;
        }
    }
    for (auto it = layout.m_boxes.begin(); it != layout.m_boxes.end(); ++it) {
        auto res = check_box(*it);
        if (res.second) return res;
    }
    return std::make_pair(nullptr, 0);
}

bill_layout_script copyLayout(const bill_layout_script &layout) {
    bill_layout_script ret;
    ret.language_code = layout.language_code;
    for (auto &ptr : layout.m_boxes) {
        ret.m_boxes.push_back(std::make_shared<layout_box>(*ptr));
    }
    return ret;
}