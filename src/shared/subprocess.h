#ifndef __SUBPROCESS_H__
#define __SUBPROCESS_H__

#include <string>
#include <memory>
#include <streambuf>
#include <iostream>


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
    virtual void close() {}
};

class pipe_istreambuf : public std::streambuf {
public:
    pipe_istreambuf(pipe_t &m_pipe) : m_pipe(m_pipe) {}

protected:
    virtual int underflow() override {
        int nbytes;
        nbytes = m_pipe.read(BUFSIZE, buffer);
        if (nbytes <= 0) {
            return std::char_traits<char>::eof();
        } else {
            setg(buffer, buffer, buffer + nbytes);
            return std::char_traits<char>::to_int_type(*gptr());
        }
    };

private:
    pipe_t &m_pipe;
    char buffer[BUFSIZE];
};

class pipe_istream : public std::istream {
public:
    pipe_istream(pipe_t &m_pipe) : std::istream(nullptr), buffer(m_pipe) {
        std::ios::init(&buffer);
    }

private:
    pipe_istreambuf buffer;
};

class pipe_ostreambuf : public std::streambuf {
public:
    pipe_ostreambuf(pipe_t &m_pipe) : m_pipe(m_pipe) {}

protected:
    virtual int overflow(int ch) override {
        if (m_pipe.write(1, &ch) == 1) {
            return ch;
        } else {
            return std::char_traits<char>::eof();
        }
    }

    virtual std::streamsize xsputn(const char_type *s, std::streamsize n) override {
        return m_pipe.write(n, s);
    }

private:
    pipe_t &m_pipe;
};

class pipe_ostream : public std::ostream {
public:
    pipe_ostream(pipe_t &m_pipe) : std::ostream(nullptr), buffer(m_pipe) {
        std::ios::init(&buffer);
    }

private:
    pipe_ostreambuf buffer;
};

class subprocess {
public:
    virtual int wait_finished() = 0;

    virtual void abort() = 0;

    virtual void close_stdin() = 0;

    virtual pipe_istream &stdout_stream() {
        return *m_stdout;
    };

    pipe_istream &stderr_stream() {
        return *m_stderr;
    }

    pipe_ostream &stdin_stream() {
        return *m_stdin;
    }

protected:
    std::unique_ptr<pipe_istream> m_stdout, m_stderr;
    std::unique_ptr<pipe_ostream> m_stdin;
};

std::unique_ptr<subprocess> open_process(const char *args[]);

#endif // __SUBPROCESS_H__