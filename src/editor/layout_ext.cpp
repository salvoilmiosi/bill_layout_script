#include "layout_ext.h"

constexpr float RESIZE_TOLERANCE = 8.f;

box_reference getBoxAt(bill_layout_script &boxes, float x, float y, int page) {
    auto check_box = [&](const layout_box &box) {
        return (x > box.x && x < box.x + box.w && y > box.y && y < box.y + box.h && box.page == page);
    };
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (it->selected && check_box(*it)) return it;
    }
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (check_box(*it)) return it;
    }
    return boxes.end();
}

std::pair<box_reference, int> getBoxResizeNode(bill_layout_script &boxes, float x, float y, int page, float scalex, float scaley) {
    float nw = RESIZE_TOLERANCE / scalex;
    float nh = RESIZE_TOLERANCE / scaley;
    auto check_box = [&](box_reference it) {
        if (it->page == page) {
            int node = 0;
            if (y > it->y - nh && y < it->y + it->h + nh) {
                if (x > it->x - nw && x < it->x + nw) {
                    node |= RESIZE_LEFT;
                } else if (x > it->x + it->w - nw && x < it->x + it->w + nw) {
                    node |= RESIZE_RIGHT;
                }
            }
            if (x > it->x - nw && x < it->x + it->w + nw) {
                if (y > it->y - nh && y < it->y + nh) {
                    node |= RESIZE_TOP;
                } else if (y > it->y + it->h - nh && y < it->y + it->h + nh) {
                    node |= RESIZE_BOTTOM;
                }
            }
            return std::make_pair(it, node);
        }
        return std::make_pair(boxes.end(), 0);
    };
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (it->selected) {
            auto res = check_box(it);
            if (res.second) return res;
        }
    }
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        auto res = check_box(it);
        if (res.second) return res;
    }
    return std::make_pair(boxes.end(), 0);
}