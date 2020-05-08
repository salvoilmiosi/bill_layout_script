#include "layout.h"

layout_bolletta::layout_bolletta() {

}

std::vector<layout_box>::iterator layout_bolletta::getBoxUnder(float x, float y, int page) {
    for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (x > it->x && x < it->x + it->w && y > it->y && y < it->y + it->h && it->page == page) {
            return it;
        }
    }
    return boxes.end();
}