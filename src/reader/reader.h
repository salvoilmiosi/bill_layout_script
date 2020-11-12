#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <stack>
#include <memory>

#include "variable.h"
#include "../compiler/assembler.h"
#include "../shared/layout.h"

struct box_spacer {
    float w;
    float h;
};

struct content_view {
    std::string text;
    size_t token_start = -1;
    size_t token_end = -1;

    content_view(const std::string &_text) : text(_text) {}
    
    void next_token(const std::string &separator);

    std::string_view view() const;
};

class reader {
public:
    void read_layout(const pdf_info &info, std::istream &input);

    const variable &get_global(const std::string &name) const;

    std::ostream &print_output(std::ostream &output, bool debug);

private:
    void exec_command(const pdf_info &info, const command_args &cmd);
    void read_box(const pdf_info &info, layout_box box);
    void call_function(const std::string &name, size_t numargs);

    const variable &get_variable(const std::string &name, size_t index) const;
    void set_variable(const std::string &name, size_t index, const variable &value);
    void reset_variable(const std::string &name, const variable &value);
    void clear_variable(const std::string &name);
    size_t get_variable_size(const std::string &name);

private:
    using variable_page = std::map<std::string, std::vector<variable>>;

    std::map<std::string, box_spacer> m_spacers;
    std::map<std::string, variable> m_globals;
    std::vector<variable_page> m_pages;

    std::vector<command_args> m_commands;
    std::vector<std::string> m_strings;
    
    std::vector<variable> m_var_stack;
    std::vector<content_view> m_content_stack;

    size_t m_page_num = 0;
    size_t m_programcounter = 0;
    bool m_jumped = false;
};

#endif