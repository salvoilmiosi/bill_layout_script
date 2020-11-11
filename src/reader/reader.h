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

private:
    std::map<std::string, box_spacer> m_spacers;
    std::map<std::string, variable> m_globals;
    std::vector<std::map<std::string, std::vector<variable>>> m_values;

    const variable &get_variable(const std::string &name) const;
    void set_variable(const std::string &name, const variable &value);
    void reset_variable(const std::string &name, const variable &value);
    void clear_variable(const std::string &name);
    size_t get_variable_size(const std::string &name);

private:
    std::vector<command_args> commands;
    std::vector<variable> var_stack;
    std::vector<content_view> content_stack;
    size_t index_reg = 0;
    size_t program_counter = 0;
    bool jumped = false;
};

#endif