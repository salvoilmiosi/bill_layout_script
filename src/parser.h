#ifndef __PARSER_H__
#define __PARSER_H__

#include <vector>
#include <map>

#include <fmt/core.h>

#include "layout.h"
#include "lexer.h"
#include "bytecode.h"
#include "stack.h"

#define VARIABLE_PREFIXES \
F(OVERWRITE) \
F(PARSENUM) \
F(AGGREGATE) \
F(MOVE) \
F(FORCE)

#define F(x) POS_VP_##x,
enum { VARIABLE_PREFIXES };
#undef F
#define F(x) VP_##x = (1 << POS_VP_##x),
enum variable_prefixes { VARIABLE_PREFIXES };
#undef F

#define PARSER_FLAGS \
F(ADD_COMMENTS) \
F(NO_EVAL_JUMPS)

#define F(x) POS_PARSER_##x,
enum { PARSER_FLAGS };
#undef F
#define F(x) PARSER_##x = (1 << POS_PARSER_##x),
enum parser_flags { PARSER_FLAGS };
#undef F

struct loop_label_pair {
    std::string continue_label;
    std::string break_label;
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

    const auto &get_comments() {
        return m_comments;
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
    template<opcode Cmd, typename ... Ts>
    void add_line(Ts && ... args) {
        if constexpr (std::is_void_v<opcode_type<Cmd>>) {
            m_code.emplace_back(Cmd);
        } else {
            m_code.emplace_back(Cmd, opcode_type<Cmd>{ std::forward<Ts>(args) ... });
        }
    }

    void add_label(const std::string &label);
    void add_comment(const std::string &comment);

private:
    const bill_layout_script *m_layout = nullptr;

    lexer m_lexer;
    bytecode m_code;
    std::multimap<size_t, std::string> m_comments;
    std::map<std::string, size_t> m_labels;

    simple_stack<loop_label_pair> m_loop_labels;

    uint8_t m_flags = 0;

    friend class lexer;
};

#endif