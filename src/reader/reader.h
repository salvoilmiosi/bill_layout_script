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

using variable_page = std::multimap<std::string, variable>;

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
    variable_page &current_page();

    variable_page m_globals;
    std::vector<variable_page> m_pages;

    friend class variable_ref;

private:
    pdf_document m_doc;
    bytecode m_code;

    template<typename T>
    using stack_t = my_stack<T>;

    stack_t<variable> m_var_stack;
    stack_t<content_view> m_content_stack;
    stack_t<variable_ref> m_ref_stack;
    box_spacer m_spacer;

    int m_box_page = 0;
    size_t m_page_num = 0;
    size_t m_programcounter = 0;
    bool m_global_flag = false;
    bool m_jumped = false;
};

#endif