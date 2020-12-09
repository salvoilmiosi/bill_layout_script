#ifdef __linux__

#include "proc_linux.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void linux_pipe::init() {
    if (::pipe(m_handles) < 0) {
        throw process_error("Errore nella creazione della pipe");
    }
}

void linux_pipe::redirect(int from, int to) {
    if (::dup2(m_handles[from], to) < 0) {
        ::exit(errno);
    }
}

int linux_pipe::write(size_t bytes, const void *buffer) {
    return ::write(m_handles[PIPE_WRITE], buffer, bytes);
}

int linux_pipe::read(size_t bytes, void *buffer) {
    return ::read(m_handles[PIPE_READ], buffer, bytes);
}

void linux_pipe::close(int which) {
    if ((which != PIPE_WRITE) && m_handles[PIPE_READ]) {
        ::close(m_handles[PIPE_READ]);
        m_handles[PIPE_READ] = 0;
    }
    if ((which != PIPE_READ) && m_handles[PIPE_WRITE]) {
        ::close(m_handles[PIPE_WRITE]);
        m_handles[PIPE_WRITE] = 0;
    }
}

linux_process::linux_process(const char *args[]) {
    pipe_stdout.init();
    pipe_stderr.init();
    pipe_stdin.init();

    child_pid = fork();
    if (child_pid == 0) {
        pipe_stdin.redirect(PIPE_READ, STDIN_FILENO);
        pipe_stdout.redirect(PIPE_WRITE, STDOUT_FILENO);
        pipe_stderr.redirect(PIPE_WRITE, STDERR_FILENO);

        pipe_stdin.close();
        pipe_stdout.close();
        pipe_stderr.close();

        int result = execvp(args[0], const_cast<char *const*>(args));

        exit(result);
    } else {
        pipe_stdout.close(PIPE_WRITE);
        pipe_stderr.close(PIPE_WRITE);
        pipe_stdin.close(PIPE_READ);

        stream_out.init(pipe_stdout);
        stream_err.init(pipe_stderr);
        stream_in.init(pipe_stdin);
    }
}

int linux_process::wait_finished() {
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

void linux_process::void abort() {
    ::kill(child_pid, SIGTERM);
}

#endif