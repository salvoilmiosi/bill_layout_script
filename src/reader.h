#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <memory>

#include "variable.h"
#include "variable_ref.h"
#include "bytecode.h"
#include "stack.h"
#include "content_view.h"

struct box_spacer {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    int page = 0;
};

using variable_map = std::multimap<std::string, variable>;

struct context {
    simple_stack<variable> vars;
    simple_stack<content_view> contents;
    simple_stack<variable_ref> refs;

    box_spacer spacer;
    int last_box_page = 0;

    size_t current_table = 0;

    size_t program_counter = 0;
    bool jumped = false;
};

struct reader_output {
    variable_map globals;
    std::vector<variable_map> values;
    std::vector<std::string> warnings;
};

class reader {
public:
    reader(const pdf_document &doc) : m_doc(doc) {}
    
    void exec_program(bytecode code);
    reader_output &get_output() {
        return m_out;
    }

    void halt();

private:
    void exec_command(const command_args &cmd);
    void set_page(int page);
    void read_box(pdf_rect box);
    void call_function(const std::string &name, size_t numargs);

private:
    const pdf_document &m_doc;
    bytecode m_code;

    context m_con;
    reader_output m_out;
};

#endif