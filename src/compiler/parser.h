#ifndef __READER_H__
#define __READER_H__

#include <vector>
#include <map>

#include <json/json.h>

#include "../shared/layout.h"
#include "tokenizer.h"

struct spacer {
    float w = 0, h = 0;
    spacer() {}
    spacer(float w, float h) : w(w), h(h) {}
};

enum variable_flags {
    FLAGS_APPEND        = 1 << 0,
    FLAGS_CLEAR         = 1 << 1,
    FLAGS_GLOBAL        = 1 << 2,
    FLAGS_NUMBER        = 1 << 3,
    FLAGS_DEBUG         = 1 << 4,
};

struct variable_ref {
    std::string name;
    uint8_t flags = 0;
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
    
    void add_value(variable_ref ref);
    void exec_line();
    void evaluate();
    void exec_function();

    variable_ref get_variable();

private:
    std::vector<std::string> output_asm;

    tokenizer tokens;
};

#endif