#ifndef __PIPE_H__
#define __PIPE_H__

#include <string>
#include <memory>

struct rwops {
    virtual int read(size_t bytes, void *buffer) { return 0; }
    virtual int write(size_t bytes, const void *buffer) { return 0; }
    virtual void close() { }

    std::string read_all();
    int write_all(const std::string &buffer);
};

std::unique_ptr<rwops> open_process(const std::string &cmd, const std::string &args, const std::string &cwd = "");

struct pipe_error {
    const char *message;

    pipe_error(const char *message) : message(message) {}
};

#endif // __PIPE_H__