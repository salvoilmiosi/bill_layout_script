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

static constexpr size_t PAGE_GLOBAL = -1;

class reader {
public:
    void open_pdf(const std::string &filename) {
        m_doc.open(filename);
    }
    
    void exec_program(std::istream &input);

    void save_output(Json::Value &output, bool debug);

    variable_ref create_ref(const std::string &name, size_t page_idx, size_t index = 0, size_t range_len = 0) {
        return variable_ref(get_page(page_idx), name, index, range_len);
    }

private:
    void exec_command(const command_args &cmd);
    void set_page(int page);
    void read_box(pdf_rect box);
    void call_function(const std::string &name, size_t numargs);

private:
    variable_page &get_page(size_t page_idx);

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