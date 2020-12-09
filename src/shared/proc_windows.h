#ifndef __PROC_WINDOWS_H__
#define __PROC_WINDOWS_H__

#if defined(WIN32) || defined(_WIN32)

#include <windows.h>
#include "proc_stream.h"

class windows_pipe : public pipe_t {
public:
    void create_pipe(SECURITY_ATTRIBUTES &attrs, int which);

    virtual int read(size_t bytes, void *buffer) override;

    virtual int write(size_t bytes, const void *buffer) override;
    
    virtual void close(int which) override;

private:
    HANDLE m_handles[2];

    friend class windows_process;
};

class windows_process : public subprocess_base {
public:
    windows_process(const char *args[]);
    ~windows_process();

public:
    int wait_finished();

    void abort() override {
        TerminateProcess(process, 1);
    }

private:
    windows_pipe pipe_stdout, pipe_stderr, pipe_stdin;
    HANDLE process = nullptr;
};

#endif
#endif