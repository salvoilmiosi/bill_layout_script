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

struct spacer {
    float w, h;
    spacer(float w, float h) : w(w), h(h) {}
};

class parser {
public:
    void read_box(const std::string &app_dir, const std::string &file_pdf, const pdf_info &info, const layout_box &box);
    void read_script(std::istream &stream, const std::string &text);
    const variable &get_variable(const std::string &name, size_t index = 0) const;

    friend std::ostream & operator << (std::ostream &out, const parser &res);

    bool debug = false;

private:
    void add_value(const std::string &name, const variable &value);
    void add_entry(const std::string &script, const std::string &value);
    void add_spacer(const std::string &script, const std::string &value, const spacer &size);
    variable evaluate(const std::string &script, const std::string &value);

private:
    std::map<std::string, std::vector<variable>> m_values;
    std::map<std::string, spacer> m_spacers;
};

#endif