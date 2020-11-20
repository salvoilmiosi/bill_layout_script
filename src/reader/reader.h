#ifndef __READER_H__
#define __READER_H__

#include <iostream>
#include <map>
#include <vector>
#include <stack>
#include <memory>
#include <json/json.h>

#include "variable.h"
#include "disassembler.h"

struct content_view {
    std::string text;
    size_t token_start = -1;
    size_t token_end = -1;

    content_view(const std::string &_text) : text(_text) {}
    
    void next_token(const std::string &separator);

    std::string_view view() const;
};

static constexpr size_t INDEX_GLOBAL = -1;

struct variable_ref {
    std::string name;
    size_t index;
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
    void read_layout(const pdf_info &info, std::istream &input);

    const variable &get_global(const std::string &name) const;

    void save_output(Json::Value &output, bool debug);

private:
    void exec_command(const pdf_info &info, const command_args &cmd);
    void read_box(const pdf_info &info, pdf_rect box);
    void call_function(const std::string &name, size_t numargs);

    const variable &get_variable() const;
    void set_variable(const variable &value);
    void reset_variable(const variable &value);
    void clear_variable();
    size_t get_variable_size();

private:
    using variable_page = std::map<std::string, std::vector<variable>>;

    std::map<std::string, variable> m_globals;
    std::vector<variable_page> m_pages;

private:
    disassembler m_asm;

    template<typename T>
    using stack_t = std::stack<T, std::vector<T>>;

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