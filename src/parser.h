#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <fmt/core.h>

#include "layout.h"
#include "lexer.h"
#include "bytecode.h"

enum variable_prefixes {
    VP_OVERWRITE   = 1 << 0,
    VP_PARSENUM    = 1 << 1,
    VP_AGGREGATE   = 1 << 2,
    VP_MOVE        = 1 << 3,
};

enum parser_flags {
    PARSER_ADD_COMMENTS = 1 << 0
};

class parser {
public:
    parser() = default;
    parser(const bill_layout_script &layout) {
        read_layout(layout);
    }

    void add_flags(parser_flags flags) {
        m_flags |= flags;
    }

    void read_layout(const bill_layout_script &layout);

    const auto &get_bytecode() {
        return m_code;
    }
    
    const auto &get_labels() {
        return m_labels;
    }

private:
    void read_box(const layout_box &box);
    const layout_box *current_box = nullptr;
    
    bool read_statement(bool throw_on_eof = true);
    void read_expression();
    void sub_expression();

    void read_function();
    void read_keyword();

    int read_variable(bool read_only = false);

private:
    template<typename ... Ts>
    void add_line(Ts && ... args) {
        m_code.emplace_back(std::forward<Ts>(args)...);
    }

    void add_label(const std::string &label);

private:
    const bill_layout_script *m_layout = nullptr;

    lexer m_lexer;
    bytecode m_code;

    std::map<std::string, size_t> m_labels;

    std::vector<std::pair<std::string, std::string>> m_loop_labels;

    uint8_t m_flags = 0;

    friend class lexer;
};

#endif