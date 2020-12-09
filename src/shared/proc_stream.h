#ifndef __PROC_STREAM_H__
#define __PROC_STREAM_H__

#include <string>
#include <streambuf>
#include <iostream>

#include "utils.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

constexpr size_t BUFSIZE = 4096;

struct process_error {
    const std::string message;

    process_error(const std::string &message) : message(message) {}
};

class pipe_t {
public:
    virtual ~pipe_t() {
        close();
    }

    virtual int read(size_t bytes, void *buffer) = 0;
    virtual int write(size_t bytes, const void *buffer) = 0;
    virtual void close(int which = -1) {}
};

class pipe_istreambuf : public std::streambuf {
public:
    void init(pipe_t &the_pipe) {
        m_pipe = &the_pipe;
    }

protected:
    virtual int underflow() override;

private:
    pipe_t *m_pipe;
    char buffer[BUFSIZE];
};

class pipe_istream : public std::istream {
public:
    void init(pipe_t &m_pipe) {
        buffer.init(m_pipe);
        std::ios::init(&buffer);
    }

private:
    pipe_istreambuf buffer;
};

class pipe_ostreambuf : public std::streambuf {
public:
    void init(pipe_t &the_pipe) {
        m_pipe = &the_pipe;
    }

protected:
    virtual int overflow(int ch) override;

    virtual int sync() override;

private:
    pipe_t *m_pipe;
    char buffer[BUFSIZE];
};

class pipe_ostream : public std::ostream {
public:
    void init(pipe_t &m_pipe) {
        buffer.init(m_pipe);
        std::ios::init(&buffer);
    }

private:
    pipe_ostreambuf buffer;
};

class subprocess_base {
public:
    virtual int wait_finished() = 0;

    virtual void abort() = 0;

    pipe_istream stream_out, stream_err;
    pipe_ostream stream_in;
};

#endif // __SUBPROCESS_H__