#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdexcept>
#include <cassert>

namespace bls {

    struct file_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct parsing_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct layout_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct conversion_error : layout_error {
        using layout_error::layout_error;
    };

    struct reader_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    struct scripted_error : reader_error {
        int errcode;

        template<typename T>
        scripted_error(T &&message, int errcode)
            : reader_error(std::forward<T>(message))
            , errcode(errcode) {}
    };

}

#endif