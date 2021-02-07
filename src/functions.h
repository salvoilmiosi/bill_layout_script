#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <functional>
#include <span>

#include "variable.h"

using arg_list = std::span<variable>;

struct function_handler : std::function<variable(arg_list&&)> {
    std::string name;
    size_t minargs;
    size_t maxargs;
};

const function_handler *find_function(const std::string &name);

#endif