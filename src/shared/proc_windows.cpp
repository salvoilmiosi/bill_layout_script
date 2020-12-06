#if defined(WIN32) || defined(_WIN32) && !defined(__linux__)

#include "subprocess.h"
#include <windows.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

class windows_process : public subprocess {
public:
    windows_process(const char *args[]);
    ~windows_process();

public:
    virtual int read_stdout(size_t bytes, void *buffer) override;
    virtual int read_stderr(size_t bytes, void *buffer) override;
    virtual int write_stdin(size_t bytes, const void *buffer) override;
    virtual void close() override;
    virtual void abort() override;

private:
    HANDLE pipe_stdin[2], pipe_stdout[2], pipe_stderr[2];
    HANDLE process = nullptr;
};

windows_process::windows_process(const char *args[]) {
    SECURITY_ATTRIBUTES attrs;
    ZeroMemory(&attrs, sizeof(SECURITY_ATTRIBUTES));

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&pipe_stdout[PIPE_READ], &pipe_stdout[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(pipe_stdout[PIPE_READ], HANDLE_FLAG_INHERIT, 0))
        throw process_error("Error creating stdout pipe");

    if (!CreatePipe(&pipe_stderr[PIPE_READ], &pipe_stderr[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(pipe_stderr[PIPE_READ], HANDLE_FLAG_INHERIT, 0))
        throw process_error("Error creating stderr pipe");

    if (!CreatePipe(&pipe_stdin[PIPE_READ], &pipe_stdin[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(pipe_stdin[PIPE_WRITE], HANDLE_FLAG_INHERIT, 0))
        throw process_error("Error creating stdin pipe");

    STARTUPINFOA start_info;
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin[PIPE_READ];
    start_info.hStdError = pipe_stderr[PIPE_WRITE];
    start_info.hStdOutput = pipe_stdout[PIPE_WRITE];
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
        CloseHandle(pipe_stdout[PIPE_WRITE]);
        CloseHandle(pipe_stderr[PIPE_WRITE]);
        CloseHandle(pipe_stdin[PIPE_READ]);
    }
}

windows_process::~windows_process() {
    if (process) CloseHandle(process);
    CloseHandle(pipe_stdin[PIPE_WRITE]);
    CloseHandle(pipe_stdout[PIPE_READ]);
    CloseHandle(pipe_stderr[PIPE_READ]);
}

void windows_process::abort() {
    TerminateProcess(process, 1);
}

int windows_process::read_stdout(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(pipe_stdout[PIPE_READ], buffer, bytes, &bytes_read, nullptr))
        return -1;
    return bytes_read;
}

int windows_process::read_stderr(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(pipe_stderr[PIPE_READ], buffer, bytes, &bytes_read, nullptr))
        return -1;
    return bytes_read;
}

int windows_process::write_stdin(size_t bytes, const void *buffer) {
    DWORD bytes_written;
    if (!WriteFile(pipe_stdin[PIPE_WRITE], buffer, bytes, &bytes_written, nullptr))
        return -1;
    return bytes_written;
}

void windows_process::close() {
    CloseHandle(pipe_stdin[PIPE_WRITE]);
}

std::unique_ptr<subprocess> open_process(const char *args[]) {
    return std::make_unique<windows_process>(args);
}

#endif