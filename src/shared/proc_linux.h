#ifndef __PROC_LINUX_H__
#define __PROC_LINUX_H__

#ifdef __linux__

#include "proc_stream.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

class linux_pipe : public pipe_t {
public:
    void create_pipe();

    void redirect(int from, int to);

    virtual int write(size_t bytes, const void *buffer) override;

    virtual int read(size_t bytes, void *buffer) override;

    virtual void close(int which) override;

private:
    int m_handles[2];
};

class linux_process : public subprocess_base {
public:
    linux_process(const char *args[]);

public:
    virtual int wait_finished() override;
    
    virtual void abort() override {
        ::kill(child_pid, SIGTERM);
    }

private:
    linux_pipe pipe_stdout, pipe_stdin, pipe_stderr;
    int child_pid;
};

#endif
#endif