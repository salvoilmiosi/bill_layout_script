#ifndef __PROC_WINDOWS_H__
#define __PROC_WINDOWS_H__

#if defined(WIN32) || defined(_WIN32)

#include "proc_stream.h"
#include <windows.h>

class windows_pipe : public pipe_t {
public:
    void init(SECURITY_ATTRIBUTES &attrs, int which);

    virtual int read(size_t bytes, void *buffer) override;

    virtual int write(size_t bytes, const void *buffer) override;
    
    virtual void close(int which = -1) override;

private:
    HANDLE m_handles[2];

    friend class windows_process;
};

class windows_process : public subprocess_base {
public:
    windows_process(const char *args[]);
    ~windows_process();

public:
    virtual int wait_finished() override;

    virtual void abort() override;

private:
    windows_pipe pipe_stdout, pipe_stderr, pipe_stdin;
    HANDLE process = nullptr;
};

#endif
#endif