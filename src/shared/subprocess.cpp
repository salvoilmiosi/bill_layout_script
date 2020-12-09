#include "subprocess.h"

int pipe_istreambuf::underflow() {
    int nbytes = m_pipe->read(BUFSIZE, buffer);
    if (nbytes <= 0) {
        return EOF;
    }
    setg(buffer, buffer, buffer + nbytes);
    return *buffer;
};

int pipe_ostreambuf::overflow(int ch) {
    if (pbase() == NULL) {
        // save one byte for next overflow
        setp(buffer, buffer + BUFSIZE - 1);
        if (ch != EOF) {
            ch = sputc(ch);
        } else {
            ch = 0;
        }
    } else {
        char* end = pptr();
        if (ch != EOF) {
            *end++ = ch;
        }
        if (m_pipe->write(end - pbase(), pbase()) <= 0) {
            ch = EOF;
        } else if (ch == EOF) {
            ch = 0;
        }
        setp(buffer, buffer + BUFSIZE - 1);
    }
    return ch;
}

int pipe_ostreambuf::sync() {
    if (pptr() != pbase()) {
        if (m_pipe->write(pptr() - pbase(), pbase()) <= 0) {
            return -1;
        }
        m_pipe->close();
    }
    return 0;
}