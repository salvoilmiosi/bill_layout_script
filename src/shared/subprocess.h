#ifndef __SUBPROCESS_H__
#define __SUBPROCESS_H__

#include <string>
#include <memory>

struct subprocess {
    virtual int read_stdout(size_t bytes, void *buffer) { return 0; }
    virtual int read_stderr(size_t bytes, void *buffer) { return 0; }
    virtual int write_stdin(size_t bytes, const void *buffer) { return 0; }
    virtual void close() { }
    virtual void abort() { }

    std::string read_all(bool from_stderr = false);
    int write_all(const std::string &buffer);
};

std::unique_ptr<subprocess> open_process(const char *args[]);

struct process_error {
    const std::string message;

    process_error(const std::string &message) : message(message) {}
};

#endif // __SUBPROCESS_H__