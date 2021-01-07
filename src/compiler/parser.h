#ifndef __READER_H__
#define __READER_H__

#include <vector>
#include <map>

#include <fmt/core.h>

#include "layout.h"
#include "tokenizer.h"

struct spacer {
    float w = 0, h = 0;
    spacer() = default;
    spacer(float w, float h) : w(w), h(h) {}
};

enum variable_flags {
    VAR_RESET       = 1 << 0,
    VAR_NUMBER      = 1 << 1,
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
    parser() : tokens(*this) {}

    void add_flags(parser_flags flag) {
        m_flags |= flag;
    }

    void read_layout(const bill_layout_script &layout);
    const auto &get_output_asm() {
        return output_asm;
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
    std::vector<std::string> output_asm;

    template<typename ... Ts>
    void add_line(const Ts & ... args) {
        output_asm.push_back(fmt::format(args ...));
    }

    tokenizer tokens;

    uint8_t m_flags = FLAGS_NONE;

    friend class tokenizer;
};

#endif