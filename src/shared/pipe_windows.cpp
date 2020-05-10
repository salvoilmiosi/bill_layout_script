#if defined(WIN32) || defined(_WIN32)

#include "pipe.h"
#include <windows.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

class windows_process_rwops : public rwops {
public:
    windows_process_rwops(const char *args[]);
    ~windows_process_rwops();

public:
    virtual int read(size_t bytes, void *buffer) override;
    virtual int write(size_t bytes, const void *buffer) override;
    virtual void close_stdin() override;
    virtual void close_stdout() override;

private:
    HANDLE pipe_stdin[2], pipe_stdout[2];
};

windows_process_rwops::windows_process_rwops(const char *args[]) {
    SECURITY_ATTRIBUTES attrs;
    ZeroMemory(&attrs, sizeof(SECURITY_ATTRIBUTES));

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&pipe_stdout[PIPE_READ], &pipe_stdout[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(pipe_stdout[PIPE_READ], HANDLE_FLAG_INHERIT, 0))
        throw pipe_error("Error creating stdout pipe");

    if (!CreatePipe(&pipe_stdin[PIPE_READ], &pipe_stdin[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(pipe_stdin[PIPE_WRITE], HANDLE_FLAG_INHERIT, 0))
        throw pipe_error("Error creating stdin pipe");

    STARTUPINFOA start_info;
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin[PIPE_READ];
    start_info.hStdError = pipe_stdout[PIPE_WRITE];
    start_info.hStdOutput = pipe_stdout[PIPE_WRITE];
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    char cmdline[1024] = "";
    for (const char **cmd = args; *cmd != nullptr; ++cmd) {
        if (cmd != args) {
            strcat(cmdline, " ");
        }
        strcat(cmdline, "\"");
        for (const char *c = *cmd; *c != '\0'; ++c) {
            switch (*c) {
            case '\\':
                strcat(cmdline, "\\");
            default:
                strncat(cmdline, c, 1);
            }
        }
        if (cmd == args) {
            strcat(cmdline, ".exe");
        }
        strcat(cmdline, "\"");
    }

    PROCESS_INFORMATION proc_info;
    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));

    if (!CreateProcessA(nullptr, cmdline, nullptr, nullptr, true,
            CREATE_NO_WINDOW, nullptr, nullptr, &start_info, &proc_info)) {
        throw pipe_error("Could not open child process");
    } else {
        CloseHandle(proc_info.hProcess);
        CloseHandle(proc_info.hThread);
        CloseHandle(pipe_stdout[PIPE_WRITE]);
        CloseHandle(pipe_stdin[PIPE_READ]);
    }
}

windows_process_rwops::~windows_process_rwops() {
    close_stdin();
    close_stdout();
}

int windows_process_rwops::read(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(pipe_stdout[PIPE_READ], buffer, bytes, &bytes_read, nullptr))
        return 0;
    return bytes_read;
}

int windows_process_rwops::write(size_t bytes, const void *buffer) {
    DWORD bytes_written;
    if (!WriteFile(pipe_stdin[PIPE_WRITE], buffer, bytes, &bytes_written, nullptr))
        return 0;
    return bytes_written;
}

void windows_process_rwops::close_stdin() {
    CloseHandle(pipe_stdin[PIPE_READ]);
    CloseHandle(pipe_stdin[PIPE_WRITE]);
}

void windows_process_rwops::close_stdout() {
    CloseHandle(pipe_stdout[PIPE_READ]);
    CloseHandle(pipe_stdout[PIPE_WRITE]);
}

std::unique_ptr<rwops> open_process(const char *args[]) {
    return std::make_unique<windows_process_rwops>(args);
}

#endif