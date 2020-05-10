#ifndef __PIPE_H__
#define __PIPE_H__

#include <string>
#include <memory>

struct rwops {
    virtual int read(size_t bytes, void *buffer) { return 0; }
    virtual int write(size_t bytes, const void *buffer) { return 0; }
    virtual void close_stdin() { }
    virtual void close_stdout() { }

    std::string read_all();
    int write_all(const std::string &buffer);
};

std::unique_ptr<rwops> open_process(const char *args[]);

struct pipe_error {
    const char *message;

    pipe_error(const char *message) : message(message) {}
};

#endif // __PIPE_H__