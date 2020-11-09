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

struct command_args_base {
    asm_command command;

    command_args_base(asm_command command) : command(command) {}
    virtual ~command_args_base() {}
};

struct box_spacer {
    float w;
    float h;
};

class reader {
public:
    void read_layout(const pdf_info &info, std::istream &input);

    const variable &get_global(const std::string &name) const;

    std::ostream &print_output(std::ostream &output, bool debug);

private:
    void exec_command(const pdf_info &info, const command_args_base &cmd);
    void read_box(const pdf_info &info, layout_box box);
    void call_function(const std::string &name, size_t numargs);

private:
    std::map<std::string, box_spacer> m_spacers;
    std::map<std::string, variable> m_globals;
    std::vector<std::map<std::string, std::vector<variable>>> m_values;

    const variable &get_variable(const std::string &name) const;
    void set_variable(const std::string &name, const variable &value);
    void clear_variable(const std::string &name);
    size_t get_variable_size(const std::string &name);

private:
    std::vector<std::unique_ptr<command_args_base>> commands;
    std::vector<variable> var_stack;
    std::vector<std::string_view> content_stack;
    std::string content_text;
    size_t index_reg;
    size_t program_counter;
    bool jumped = true;
};

#endif