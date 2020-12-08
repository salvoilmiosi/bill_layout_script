#include "subprocess.h"

int pipe_istreambuf::underflow() {
    int nbytes;
    nbytes = m_pipe.read(BUFSIZE, buffer);
    if (nbytes <= 0) {
        return std::char_traits<char>::eof();
    } else {
        setg(buffer, buffer, buffer + nbytes);
        return std::char_traits<char>::to_int_type(*gptr());
    }
};

int pipe_ostreambuf::overflow(int ch) {
    if (m_pipe.write(1, &ch) == 1) {
        return ch;
    } else {
        return std::char_traits<char>::eof();
    }
}

std::streamsize pipe_ostreambuf::xsputn(const char_type *s, std::streamsize n) {
    return m_pipe.write(n, s);
}