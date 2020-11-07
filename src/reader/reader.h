#ifndef __READER_H__
#define __READER_H__

#include <vector>
#include <map>

#include <json/json.h>

#include "../shared/layout.h"
#include "variable.h"
#include "tokenizer.h"

struct parsing_error {
    const std::string message;
    const std::string line;

    parsing_error(const std::string &message, const std::string &line) : message(message), line(line) {}
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

class reader {
public:
    void read_layout(const pdf_info &info, const bill_layout_script &layout);
    const variable &get_global(const std::string &name) const;
    std::ostream &print_output(std::ostream &out, bool debug = false);

private:
    void read_box(const pdf_info &info, layout_box box);
    
    variable add_value(variable_ref ref, variable var, bool ignore = false);
    variable exec_line(tokenizer &tokens, const box_content &content, bool ignore = false);
    variable evaluate(tokenizer &tokens, const box_content &content, bool ignore = false);
    variable exec_function(tokenizer &tokens, const box_content &content, bool ignore = false);
    void exec_sys_function(tokenizer &tokens, const box_content &content, bool ignore = false);
    variable_ref get_variable(tokenizer &tokens, const box_content &content);

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