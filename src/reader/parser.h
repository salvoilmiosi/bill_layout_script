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
    float w = 0, h = 0;
    spacer() {}
    spacer(float w, float h) : w(w), h(h) {}
};

struct box_content : public layout_box {
    std::string text;

    box_content() {}
    box_content(const layout_box &from) : layout_box(from) {}
    box_content(const std::string &text) : text(text) {}
    box_content(const layout_box &from, const std::string &text) : layout_box(from), text(text) {}
};

class parser {
public:
    void read_layout(const std::string &file_pdf, const bill_layout_script &layout);
    void read_box(const std::string &file_pdf, const pdf_info &info, layout_box box);
    void read_script(std::istream &stream, const std::string &text);
    const variable &get_variable(const std::string &name) const;

    friend std::ostream & operator << (std::ostream &out, const parser &res);

    bool debug = false;

private:
    using variable_page = std::map<std::string, std::vector<variable>>;
    variable_page &get_variable_page();
    void add_value(const std::string &name, const variable &value);
    void execute_line(const std::string &script, const box_content &content);
    variable evaluate(const std::string &script, const box_content &content);

private:
    size_t program_counter = 0;
    bool jumped = false;
    std::vector<variable_page> m_values;
    std::map<std::string, spacer> m_spacers;
    std::map<std::string, size_t> goto_labels;
    std::map<std::string, variable> m_globals;
};

#endif