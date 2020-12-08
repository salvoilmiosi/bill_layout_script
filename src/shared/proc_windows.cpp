#if defined(WIN32) || defined(_WIN32) && !defined(__linux__)

#include "subprocess.h"
#include <windows.h>

class windows_pipe : public pipe_t {
public:
    void create_pipe(SECURITY_ATTRIBUTES &attrs, bool is_write);

    virtual int read(size_t bytes, void *buffer) override;
    virtual int write(size_t bytes, const void *buffer) override;
    
    virtual void close() override {
        CloseHandle(m_read);
        CloseHandle(m_write);
    }

private:
    HANDLE m_read;
    HANDLE m_write;

    friend class windows_process;
};

void windows_pipe::create_pipe(SECURITY_ATTRIBUTES &attrs, bool is_write) {
    if (!CreatePipe(&m_read, &m_write, &attrs, 0)
        || !SetHandleInformation(is_write ? m_write : m_read, HANDLE_FLAG_INHERIT, 0))
        throw process_error("Error creating pipe");
}

int windows_pipe::read(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(m_read, buffer, bytes, &bytes_read, nullptr))
        return -1;
    return bytes_read;
}

int windows_pipe::write(size_t bytes, const void *buffer) {
    DWORD bytes_written;
    if (!WriteFile(m_write, buffer, bytes, &bytes_written, nullptr))
        return -1;
    return bytes_written;
}

class windows_process : public subprocess {
public:
    windows_process(const char *args[]);
    ~windows_process();

public:
    virtual int wait_finished() override;
    
    virtual void close_stdin() override {
        pipe_stdin.close();
    }

    virtual void abort() override {
        TerminateProcess(process, 1);
    }

private:
    windows_pipe pipe_stdout, pipe_stderr, pipe_stdin;
    HANDLE process = nullptr;
};

windows_process::windows_process(const char *args[]) {
    SECURITY_ATTRIBUTES attrs;
    ZeroMemory(&attrs, sizeof(SECURITY_ATTRIBUTES));

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    pipe_stdout.create_pipe(attrs, false);
    pipe_stderr.create_pipe(attrs, false);
    pipe_stdin.create_pipe(attrs, true);

    STARTUPINFOA start_info;
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin.m_read;
    start_info.hStdError = pipe_stderr.m_write;
    start_info.hStdOutput = pipe_stdout.m_write;
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmdline;
    for (const char **cmd = args; *cmd != nullptr; ++cmd) {
        if (cmd != args) {
            cmdline += ' ';
        }
        cmdline += '\"';
        for (const char *c = *cmd; *c != '\0'; ++c) {
            switch (*c) {
            case '\\':
                cmdline += '\\';
                // fall through
            default:
                cmdline += *c;
            }
        }
        if (cmd == args) {
            cmdline += ".exe";
        }
        cmdline += '\"';
    }

    PROCESS_INFORMATION proc_info;
    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));

    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, true,
            CREATE_NO_WINDOW, nullptr, nullptr, &start_info, &proc_info)) {
        throw process_error("Could not open child process");
    } else {
        process = proc_info.hProcess;
        CloseHandle(proc_info.hThread);
        CloseHandle(pipe_stdout.m_write);
        CloseHandle(pipe_stderr.m_write);
        CloseHandle(pipe_stdin.m_read);

        m_stdout.init(pipe_stdout);
        m_stderr.init(pipe_stderr);
        m_stdin.init(pipe_stdin);
    }
}

windows_process::~windows_process() {
    CloseHandle(process);
}

int windows_process::wait_finished() {
    WaitForSingleObject(process, INFINITE);
    DWORD exit_code;
    if (!GetExitCodeProcess(process, &exit_code)) {
        throw process_error("Impossibile ottenere exit code");
    }
    return exit_code;
}

std::unique_ptr<subprocess> open_process(const char *args[]) {
    return std::make_unique<windows_process>(args);
}

#endif