#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>
#include <cstring>

#define RESIZE_TOP      1 << 0
#define RESIZE_BOTTOM   1 << 1
#define RESIZE_LEFT     1 << 2
#define RESIZE_RIGHT    1 << 4

struct layout_box : public pdf_rect {
    bool selected = false;
    char name[128];
    char script[10240];
    char spacers[128];
    char goto_label[128];
    bool whole_file = false;

    layout_box() {
        memset(name, 0, sizeof(name));
        memset(script, 0, sizeof(script));
        memset(spacers, 0, sizeof(spacers));
        memset(goto_label, 0, sizeof(goto_label));
    }
};

struct layout_error {
    const char *message;

    layout_error(const char *message) : message(message) {}
};

using box_reference = std::vector<layout_box>::iterator;

class bill_layout_script {
public:
    bill_layout_script();
    box_reference getBoxAt(float x, float y, int page);
    std::pair<box_reference, int> getBoxResizeNode(float x, float y, int page, float scalex, float scaley);

    void clear() {
        boxes.clear();
    }

public:
    std::vector<layout_box> boxes;
};

std::ostream &operator << (std::ostream &out, const bill_layout_script &obj);
std::istream &operator >> (std::istream &in, bill_layout_script &obj);

#endif