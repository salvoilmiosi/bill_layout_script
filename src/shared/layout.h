#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>

enum box_type { 
    BOX_SINGLE,
    BOX_MULTIPLE,
    BOX_COLUMNS,
    BOX_ROWS,
};

struct layout_box : public xpdf::rect {
    bool selected = false;
    std::string name = "";
    std::string parse_string = "";
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

    void newFile();
    void saveFile(const std::string &filename);
    void openFile(const std::string &filename);

public:
    std::vector<layout_box> boxes;
};

#endif