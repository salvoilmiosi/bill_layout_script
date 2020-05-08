#ifndef __BOX_LAYOUT_H__
#define __BOX_LAYOUT_H__

#include "xpdf.h"

#include <vector>

enum box_type { 
    BOX_STRING,
    BOX_NUMBER,
    BOX_NUMBER_ARRAY,
    BOX_NUMBER_GRID,
};

struct layout_box : public xpdf::rect {
    bool selected = false;
    std::string name = "";
    std::string parse_string = "";
    box_type type = BOX_STRING;
};

struct layout_error {
    const char *message;

    layout_error(const char *message) : message(message) {}
};

class layout_bolletta {
public:
    layout_bolletta();
    std::vector<layout_box>::iterator getBoxUnder(float x, float y, int page);

    void newFile();
    void saveFile(const std::string &filename);
    void openFile(const std::string &filename);

public:
    std::vector<layout_box> boxes;
};

#endif