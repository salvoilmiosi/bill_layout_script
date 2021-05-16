#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdexcept>

struct layout_error : std::runtime_error {
    template<typename T>
    layout_error(T &&message) : std::runtime_error(std::forward<T>(message)) {}
};

struct layout_runtime_error : std::runtime_error {
    int errcode;

    template<typename T>
    layout_runtime_error(T &&message, int errcode)
        : std::runtime_error(std::forward<T>(message))
        , errcode(errcode) {}
};

#endif