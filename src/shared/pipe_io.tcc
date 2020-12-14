#include "pipe_io.h"

template<typename pipe_t>
std::streambuf::int_type pipe_istreambuf<pipe_t>::underflow() {
    int nbytes = m_pipe.read(BUFSIZE, buffer);
    if (nbytes <= 0) {
        return traits_type::eof();
    }
    setg(buffer, buffer, buffer + nbytes);
    return traits_type::to_int_type(*gptr());
}

template<typename pipe_t>
std::streambuf::int_type pipe_ostreambuf<pipe_t>::overflow(std::streambuf::int_type ch) {
    int write = pptr() - pbase();
    if (write && m_pipe.write(pptr() - pbase(), pbase()) != write) {
        return traits_type::eof();
    }

    setp(buffer, buffer + BUFSIZE);
    if (!traits_type::eq_int_type(ch, traits_type::eof())) sputc(ch);
    return traits_type::not_eof(ch);
}

template<typename pipe_t>
int pipe_ostreambuf<pipe_t>::sync() {
    return pptr() != pbase() && m_pipe.write(pptr() - pbase(), pbase()) <= 0 ? -1 : 0;
}