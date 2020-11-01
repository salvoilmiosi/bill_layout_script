#ifdef __linux__

#include "pipe.h"
#include <unistd.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

class unix_process_rwops : public rwops {
public:
    unix_process_rwops(const char *args[]);
    ~unix_process_rwops();

public:
    virtual int read(size_t bytes, void *buffer) override;
    virtual int write(size_t bytes, const void *buffer) override;
    virtual void close_stdin() override;
    virtual void close_stdout() override;

private:
    int pipe_stdin[2], pipe_stdout[2];
    int child_pid;
};

unix_process_rwops::unix_process_rwops(const char *args[]) {
    if (pipe(pipe_stdout) < 0)
        throw pipe_error("Error creating stdout pipe");

    if (pipe(pipe_stdin) < 0) {
        ::close(pipe_stdout[PIPE_READ]);
        ::close(pipe_stdout[PIPE_WRITE]);

        throw pipe_error("Error creating stdin pipe");
    }

    child_pid = fork();
    if (child_pid == 0) {
        if (dup2(pipe_stdin[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
        if (dup2(pipe_stdout[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
        if (dup2(pipe_stdout[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);

        ::close(pipe_stdin[PIPE_READ]);
        ::close(pipe_stdin[PIPE_WRITE]);
        ::close(pipe_stdout[PIPE_READ]);
        ::close(pipe_stdout[PIPE_WRITE]);

        int result = execv(args[0], const_cast<char *const*>(args));

        exit(result);
    } else {
        ::close(pipe_stdout[PIPE_WRITE]);
        ::close(pipe_stdin[PIPE_READ]);
    }
}

unix_process_rwops::~unix_process_rwops() {
    close_stdin();
    close_stdout();
}

int unix_process_rwops::read(size_t bytes, void *buffer) {
    return ::read(pipe_stdout[PIPE_READ], buffer, bytes);
}

int unix_process_rwops::write(size_t bytes, const void *buffer) {
    return ::write(pipe_stdin[PIPE_WRITE], buffer, bytes);
}

void unix_process_rwops::close_stdin() {
    ::close(pipe_stdin[PIPE_WRITE]);
}

void unix_process_rwops::close_stdout() {
    ::close(pipe_stdout[PIPE_READ]);
}

std::unique_ptr<rwops> open_process(const char *args[]) {
    return std::make_unique<unix_process_rwops>(args);
}

#endif