#ifndef __READER_H__
#define __READER_H__

#include <vector>
#include <map>

#include <json/json.h>
#include <fmt/core.h>

#include "../shared/layout.h"
#include "tokenizer.h"

struct spacer {
    float w = 0, h = 0;
    spacer() {}
    spacer(float w, float h) : w(w), h(h) {}
};

enum variable_flags {
    VAR_APPEND        = 1 << 0,
    VAR_CLEAR         = 1 << 1,
    VAR_NUMBER        = 1 << 2,
    VAR_DEBUG         = 1 << 3,
};

class parser {
public:
    void read_layout(const bill_layout_script &layout);
    const auto &get_output_asm() {
        return output_asm;
    }

private:
    void read_box(const layout_box &box);
    const layout_box *current_box = nullptr;
    
    void read_statement();
    void read_expression();
    void read_function();

    int read_variable();

private:
    std::vector<std::string> output_asm;

    template<typename ... Ts>
    void add_line(const Ts & ... args) {
        output_asm.push_back(fmt::format(args ...));
    }

    tokenizer tokens;
};

#endif