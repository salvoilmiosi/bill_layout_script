#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <json/json.h>

#include "../shared/layout.h"
#include "variable.h"

class parser {
public:
    void read_box(const layout_box &box, const std::string &text);

    friend std::ostream & operator << (std::ostream &out, const parser &res);

    bool debug = false;

private:
    variable evaluate(const std::string &script);

    void add_entry(const std::string &script, const std::string &value);

private:
    std::map<std::string, std::vector<variable>> m_values;
};

#endif