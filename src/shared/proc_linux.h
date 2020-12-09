#ifndef __PROC_LINUX_H__
#define __PROC_LINUX_H__

#ifdef __linux__

#include "pipe_io.h"

class linux_pipe {
public:
    linux_pipe();
    
    void redirect(int from, int to);

    ~linux_pipe() {
        close();
    }

    int write(size_t bytes, const void *buffer);
    int read(size_t bytes, void *buffer);
    void close(int which = -1);

private:
    int m_handles[2];
};

class linux_process {
public:
    linux_process(const char *args[]);

public:
    int wait_finished();
    void abort();

private:
    linux_pipe pipe_stdout, pipe_stdin, pipe_stderr;
    int child_pid;

public:
    pipe_istream<linux_pipe> stream_out, stream_err;
    pipe_ostream<linux_pipe> stream_in;
};

#endif
#endif