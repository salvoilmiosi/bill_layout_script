#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <fmt/core.h>

#include "layout.h"
#include "lexer.h"
#include "bytecode.h"

struct spacer {
    float w = 0, h = 0;
    spacer() = default;
    spacer(float w, float h) : w(w), h(h) {}
};

enum variable_flags {
    VAR_RESET       = 1 << 0,
    VAR_PARSENUM    = 1 << 1,
    VAR_INCREASE    = 1 << 2,
    VAR_DECREASE    = 1 << 3,
    VAR_MOVE        = 1 << 4,
};

enum parser_flags {
    FLAGS_NONE = 0,
    FLAGS_DEBUG = 1 << 0,
};

class parser {
public:
    parser() = default;
    parser(const bill_layout_script &layout) {
        read_layout(layout);
    }

    void add_flags(parser_flags flag) {
        m_flags |= flag;
    }

    void read_layout(const bill_layout_script &layout);

    const auto &get_lines() {
        return m_lines;
    }

private:
    void read_box(const layout_box &box);
    const layout_box *current_box = nullptr;
    
    bool read_statement();
    void read_expression();
    void sub_expression();

    void read_function();
    void read_keyword();
    void read_date_fun(const std::string &fun_name);

    int read_variable(bool read_only = false);

private:
    std::vector<std::string> m_lines;

    template<typename ... Ts>
    void add_line(const Ts & ... args) {
        m_lines.push_back(fmt::format(args ...));
    }

    lexer m_lexer{*this};

    uint8_t m_flags = FLAGS_NONE;

    friend class lexer;
};

#endif