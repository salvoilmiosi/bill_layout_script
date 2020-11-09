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
    std::string name;
    std::string script;
    std::string spacers;
    std::string goto_label;
};

struct layout_error {
    std::string message;

    layout_error(const std::string &message) : message(message) {}
};

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line) : message(message), line(line) {}
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