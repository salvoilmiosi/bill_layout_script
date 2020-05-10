#ifndef __PIPE_H__
#define __PIPE_H__

#include <string>
#include <memory>

#include "binary_io.h"

std::unique_ptr<rwops> open_process(char *const args[]);

struct pipe_error {
    const char *message;

    pipe_error(const char *message) : message(message) {}
};

#endif // __PIPE_H__