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

struct box_spacer {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    int page = 0;
};

enum set_flags {
    SET_ASSIGN = 0,
    SET_RESET = 1 << 0,
    SET_INCREASE = 1 << 1
};

class variable_ref {
private:
    class reader &parent;

public:
    std::string name;
    size_t index = 0;
    size_t range_len = 0;
    size_t page_idx = 0;

public:
    explicit variable_ref(class reader &parent) : parent(parent) {}
    variable_ref(class reader &parent, const std::string &name, size_t page_idx, size_t index = 0, size_t range_len = 0) :
        parent(parent), name(name), index(index), range_len(range_len), page_idx(page_idx) {}

public:
    const variable &get_value() const;
    void set_value(variable &&value, set_flags flags = SET_ASSIGN);
    void clear();
    size_t size() const;
    bool isset() const;
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
        return variable_ref(*this, name, page_idx, index, range_len);
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
    bool m_jumped = false;
};

#endif