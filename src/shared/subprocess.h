#ifndef __SUBPROCESS_H__
#define __SUBPROCESS_H__

#include <string>
#include <memory>
#include <streambuf>

constexpr size_t BUFSIZE = 4096;

class subprocess {
public:
    virtual int read_stdout(size_t bytes, void *buffer) { return 0; }
    virtual int read_stderr(size_t bytes, void *buffer) { return 0; }
    virtual int write_stdin(size_t bytes, const void *buffer) { return 0; }

    virtual void close_stdin() { }
    virtual void abort() { }

    std::string read_all_error();
    std::string read_all();
    int write_all(const std::string &buffer);

    void write_to(subprocess &proc);
};

std::unique_ptr<subprocess> open_process(const char *args[]);

struct process_error {
    const std::string message;

    process_error(const std::string &message) : message(message) {}
};

#endif // __SUBPROCESS_H__