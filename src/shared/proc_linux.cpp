#ifdef __linux__

#include "subprocess.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

class unix_process : public subprocess {
public:
    unix_process(const char *args[]);
    ~unix_process();

public:
    virtual int read_stdout(size_t bytes, void *buffer) override;
    virtual int read_stderr(size_t bytes, void *buffer) override;
    virtual int write_stdin(size_t bytes, const void *buffer) override;

    virtual void close_stdin() override;
    virtual void abort() override;

private:
    int pipe_stdin[2], pipe_stdout[2], pipe_stderr[2];
    int child_pid;
};

unix_process::unix_process(const char *args[]) {
    if (pipe(pipe_stdout) < 0)
        throw process_error("Error creating stdout pipe");

    if (pipe(pipe_stderr) < 0)
        throw process_error("Error creating stderr pipe");
    
    if (pipe(pipe_stdin) < 0)
        throw process_error("Error creating stdin pipe");

    child_pid = fork();
    if (child_pid == 0) {
        if (dup2(pipe_stdin[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
        if (dup2(pipe_stdout[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
        if (dup2(pipe_stderr[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);

        ::close(pipe_stdin[PIPE_READ]);
        ::close(pipe_stdin[PIPE_WRITE]);
        ::close(pipe_stdout[PIPE_READ]);
        ::close(pipe_stdout[PIPE_WRITE]);
        ::close(pipe_stderr[PIPE_READ]);
        ::close(pipe_stderr[PIPE_WRITE]);

        int result = execvp(args[0], const_cast<char *const*>(args));

        exit(result);
    } else {
        ::close(pipe_stdout[PIPE_WRITE]);
        ::close(pipe_stderr[PIPE_WRITE]);
        ::close(pipe_stdin[PIPE_READ]);
    }
}

unix_process::~unix_process() {
    ::close(pipe_stdin[PIPE_WRITE]);
    ::close(pipe_stdout[PIPE_READ]);
    ::close(pipe_stderr[PIPE_READ]);
}

int unix_process::read_stdout(size_t bytes, void *buffer) {
    return ::read(pipe_stdout[PIPE_READ], buffer, bytes);
}

int unix_process::read_stderr(size_t bytes, void *buffer) {
    return ::read(pipe_stderr[PIPE_READ], buffer, bytes);
}

int unix_process::write_stdin(size_t bytes, const void *buffer) {
    return ::write(pipe_stdin[PIPE_WRITE], buffer, bytes);
}

void unix_process::close_stdin() {
    ::close(pipe_stdin[PIPE_WRITE]);
}

void unix_process::abort() {
    ::kill(child_pid, SIGTERM);
}

std::unique_ptr<subprocess> open_process(const char *args[]) {
    return std::make_unique<unix_process>(args);
}

#endif