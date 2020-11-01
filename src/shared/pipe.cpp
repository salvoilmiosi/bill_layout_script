#include "pipe.h"

#include <algorithm>

constexpr size_t BUFSIZE = 4096;

std::string rwops::read_all() {
    std::string out;

    char buffer[BUFSIZE];

    while (true) {
        size_t bytes_read = read(BUFSIZE, buffer);
        if (bytes_read <= 0) break;

        out.append(buffer, bytes_read);
    }

    return out;
}

int rwops::write_all(const std::string &buffer) {
    size_t bytes_written = 0;

    auto it_begin = buffer.begin();
    while (it_begin != buffer.end()) {
        auto it_end = it_begin + BUFSIZE;
        if (it_end > buffer.end()) {
            it_end = buffer.end();
        }

        bytes_written += write(it_end - it_begin, &(*it_begin));

        it_begin = it_end;
    }

    return bytes_written;
}