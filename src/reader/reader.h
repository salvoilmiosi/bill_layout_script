#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <json/json.h>

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

template<typename T>
using stack_t = my_stack<T>;

struct context {
    stack_t<variable> vars;
    stack_t<content_view> contents;
    stack_t<variable_ref> refs;

    box_spacer spacer;
    int last_box_page = 0;

    size_t current_vmap = 0;

    size_t program_counter = 0;
    bool jumped = false;
};

class reader {
public:
    void open_pdf(const std::string &filename) {
        m_doc.open(filename);
    }
    
    void exec_program(std::istream &input);

    void save_output(Json::Value &output, bool debug);

    const variable &get_global(const std::string &name, size_t index = 0) {
        return variable_ref(m_globals, name, index).get_value();
    }

private:
    void exec_command(const command_args &cmd);
    void set_page(int page);
    void read_box(pdf_rect box);
    void call_function(const std::string &name, size_t numargs);

private:
    variable_map m_globals;
    std::vector<variable_map> m_values;

private:
    pdf_document m_doc;
    bytecode m_code;
    context m_con;
};

#endif