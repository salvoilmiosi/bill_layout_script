#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <json/json.h>

#include "variable.h"
#include "bytecode.h"
#include "stack.h"
#include "content_view.h"

enum variable_flags {
    VAR_GLOBAL = 1 << 0,
    VAR_RANGE_ALL = 1 << 1,
};

struct variable_ref {
    std::string name;
    size_t index_first = 0;
    size_t index_last = 0;
    int flags = 0;
};

struct box_spacer {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    int page = 0;
};

class reader {
public:
    void open_pdf(const std::string &filename) {
        m_doc.open(filename);
    }
    
    void exec_program(std::istream &input);

    const variable &get_global(const std::string &name) const;
    const variable &get_variable(const std::string &name, size_t index = 0, size_t page_idx = 0) const;

    void save_output(Json::Value &output, bool debug);

private:
    void exec_command(const command_args &cmd);
    void set_page(int page);
    void read_box(pdf_rect box);
    void call_function(const std::string &name, size_t numargs);

    const variable &get_ref() const;
    void set_ref(bool reset = false);
    void inc_ref(const variable &value);
    void clear_ref();
    size_t get_ref_size();

private:
    using variable_page = std::map<std::string, std::vector<variable>>;

    std::map<std::string, variable> m_globals;
    std::vector<variable_page> m_pages;

private:
    pdf_document m_doc;
    bytecode m_code;

    template<typename T>
    using stack_t = my_stack<T>;

    stack_t<variable> m_var_stack;
    stack_t<content_view> m_content_stack;
    stack_t<variable_ref> m_ref_stack;
    box_spacer m_spacer;

    size_t m_page_num = 0;
    size_t m_programcounter = 0;
    bool m_jumped = false;
    bool m_ate = false;
};

#endif