#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>
#include <string>

enum box_type { 
    BOX_SINGLE,
    BOX_MULTIPLE,
    BOX_COLUMNS,
    BOX_ROWS,
};

struct layout_box : public xpdf::rect {
    bool selected = false;
    std::string name = "";
    std::string script = "";
    box_type type = BOX_SINGLE;
};

struct layout_error {
    const char *message;

    layout_error(const char *message) : message(message) {}
};

class layout_bolletta {
public:
    layout_bolletta();
    std::vector<layout_box>::iterator getBoxAt(float x, float y, int page);

    void clear() {
        boxes.clear();
    }

public:
    std::vector<layout_box> boxes;

    std::string pdf_filename{};
};

std::ostream &operator << (std::ostream &out, const layout_bolletta &obj);
std::istream &operator >> (std::istream &in, layout_bolletta &obj);

#endif