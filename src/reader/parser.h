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

class variable_ref {
private:
    class parser &parent;

public:
    static const size_t INDEX_APPEND = -1;
    static const size_t INDEX_CLEAR = -2;
    static const size_t INDEX_GLOBAL = -3;

    variable_ref(class parser &parent) : parent(parent) {};

    size_t pageidx = 0;
    std::string name;
    size_t index = 0;

    void clear();
    size_t size() const;
    bool isset() const;
    variable &operator *();
};

class parser {
public:
    void read_layout(const pdf_info &info, const bill_layout_script &layout);
    const variable &get_global(const std::string &name) const;
    std::ostream &print_output(std::ostream &out, bool debug = false);

private:
    void read_box(const pdf_info &info, layout_box box);
    void execute_script(const box_content &content);

    variable execute_line(const std::string &script, const box_content &content);
    variable evaluate(const std::string &script, const box_content &content);
    variable add_value(std::string_view name, variable value, const box_content &content);
    
    variable_ref get_variable(std::string_view name, const box_content &content);

private:
    using variable_page = std::map<std::string, std::vector<variable>>;

    size_t program_counter = 0;
    size_t return_address = -1;
    bool jumped = false;
    std::vector<variable_page> m_values;
    std::map<std::string, spacer> m_spacers;
    std::map<std::string, size_t> goto_labels;
    std::map<std::string, variable> m_globals;

    friend class variable_ref;
};

#endif