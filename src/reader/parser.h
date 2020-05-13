#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <json/json.h>

#include "../shared/layout.h"
#include "variable.h"

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line = "") : message(message), line(line) {}
};

class parser {
public:
    void read_box(const layout_box &box, const std::string &text);
    void read_script(std::istream &stream, const std::string &text);
    std::string get_file_layout();

    friend std::ostream & operator << (std::ostream &out, const parser &res);

    bool debug = false;

private:
    void add_value(const std::string &name, const variable &value);
    void add_entry(const std::string &script, const std::string &value);
    variable evaluate(const std::string &script, const std::string &value);

private:
    std::map<std::string, std::vector<variable>> m_values;
};

#endif