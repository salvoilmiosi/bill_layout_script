#include "subprocess.h"

#include <algorithm>

constexpr size_t BUFSIZE = 4096;

std::string subprocess::read_all(bool from_stderr) {
    std::string out;

    char buffer[BUFSIZE];

    while (true) {
        int bytes_read;
        if (from_stderr) {
            bytes_read = read_stderr(BUFSIZE, buffer);
        } else {
            bytes_read = read_stdout(BUFSIZE, buffer);
        }
        if (bytes_read < 0) {
            throw process_error("Impossibile leggere da processo");
        } else if (bytes_read == 0) {
            break;
        }

        out.append(buffer, bytes_read);
    }

    return out;
}

int subprocess::write_all(const std::string &buffer) {
    size_t bytes_written = 0;

    auto it_begin = buffer.begin();
    while (it_begin != buffer.end()) {
        auto it_end = it_begin + BUFSIZE;
        if (it_end > buffer.end()) {
            it_end = buffer.end();
        }

        int nbytes = write_stdin(it_end - it_begin, &(*it_begin));
        if (nbytes < 0) {
            throw process_error("Impossibile scrivere in processo");
        }
        bytes_written += nbytes;

        it_begin = it_end;
    }

    return bytes_written;
}