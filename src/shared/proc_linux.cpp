#ifdef __linux__

#include "subprocess.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

class unix_pipe : public pipe_t {
public:
    virtual int write(size_t bytes, const void *buffer) override {
        return ::write(m_handles[PIPE_WRITE], buffer, bytes);
    }

    virtual int read(size_t bytes, void *buffer) override {
        return ::read(m_handles[PIPE_READ], buffer, bytes);
    }

    virtual void close(int which) override {
        if ((which != PIPE_WRITE) && m_handles[PIPE_READ]) {
            ::close(m_handles[PIPE_READ]);
            m_handles[PIPE_READ] = 0;
        }
        if ((which != PIPE_READ) && m_handles[PIPE_WRITE]) {
            ::close(m_handles[PIPE_WRITE]);
            m_handles[PIPE_WRITE] = 0;
        }
    }

private:
    int m_handles[2];

    friend class unix_process;
};

class unix_process : public subprocess {
public:
    unix_process(const char *args[]);

public:
    virtual int wait_finished() override;
    
    virtual void abort() override {
        ::kill(child_pid, SIGTERM);
    }

private:
    unix_pipe pipe_stdout, pipe_stdin, pipe_stderr;
    int child_pid;
};

unix_process::unix_process(const char *args[]) {
    if (pipe(pipe_stdout.m_handles) < 0)
        throw process_error("Error creating stdout pipe");

    if (pipe(pipe_stderr.m_handles) < 0)
        throw process_error("Error creating stderr pipe");
    
    if (pipe(pipe_stdin.m_handles) < 0)
        throw process_error("Error creating stdin pipe");

    child_pid = fork();
    if (child_pid == 0) {
        if (dup2(pipe_stdin.m_handles[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
        if (dup2(pipe_stdout.m_handles[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
        if (dup2(pipe_stderr.m_handles[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);

        pipe_stdin.close();
        pipe_stdout.close();
        pipe_stderr.close();

        int result = execvp(args[0], const_cast<char *const*>(args));

        exit(result);
    } else {
        pipe_stdout.close(PIPE_WRITE);
        pipe_stderr.close(PIPE_WRITE);
        pipe_stdin.close(PIPE_READ);

        m_stdout.init(pipe_stdout);
        m_stderr.init(pipe_stderr);
        m_stdin.init(pipe_stdin);
    }
}

int unix_process::wait_finished() {
    int status;
    if (::waitpid(child_pid, &status, 0) == -1) {
        throw process_error("Errore in waitpid");
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else {
        throw process_error("Impossibile ottenere exit code");
    }
}

std::unique_ptr<subprocess> open_process(const char *args[]) {
    return std::make_unique<unix_process>(args);
}

#endif