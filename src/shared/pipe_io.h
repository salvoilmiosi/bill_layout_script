#ifndef __PIPE_IO_H__
#define __PIPE_IO_H__

#include <string>
#include <streambuf>
#include <iostream>
#include <algorithm>

#define PIPE_READ 0
#define PIPE_WRITE 1

constexpr size_t BUFSIZE = 4096;

struct process_error {
    const std::string message;

    process_error(const std::string &message) : message(message) {}
};

template<typename pipe_t>
class pipe_istreambuf : public std::streambuf {
public:
    pipe_istreambuf(pipe_t &m_pipe) : m_pipe(m_pipe) {}

protected:
    virtual std::streambuf::int_type underflow() override;

private:
    pipe_t &m_pipe;
    char buffer[BUFSIZE];

    bool m_clear_cr = false;

    template<typename T>
    friend class pipe_istream;
};

template<typename pipe_t>
class pipe_istream : public std::istream {
public:
    pipe_istream(pipe_t &m_pipe) : buffer(m_pipe) {
        std::ios::init(&buffer);
    }

    void close() {
        buffer.m_pipe.close();
    }

    void set_clear_cr(bool value) {
        buffer.m_clear_cr = value;
    }

private:
    pipe_istreambuf<pipe_t> buffer;
};

template<typename pipe_t>
class pipe_ostreambuf : public std::streambuf {
public:
    pipe_ostreambuf(pipe_t &m_pipe) : m_pipe(m_pipe) {}

protected:
    virtual std::streambuf::int_type overflow(std::streambuf::int_type ch) override;

    virtual int sync() override;

private:
    pipe_t &m_pipe;
    char buffer[BUFSIZE];

    template<typename T>
    friend class pipe_ostream;
};

template<typename pipe_t>
class pipe_ostream : public std::ostream {
public:
    pipe_ostream(pipe_t &m_pipe) : buffer(m_pipe) {
        std::ios::init(&buffer);
    }

    void close() {
        flush();
        buffer.m_pipe.close();
    }

private:
    pipe_ostreambuf<pipe_t> buffer;
};

#include "pipe_io.tcc"

#endif // __SUBPROCESS_H__