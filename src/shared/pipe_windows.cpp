#if defined(WIN32) || defined(_WIN32)

#include "pipe.h"
#include <windows.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

class windows_process_rwops : public rwops {
public:
    windows_process_rwops(char *const args[]);
    ~windows_process_rwops();

public:
    virtual int read(size_t bytes, void *buffer) override;
    virtual int write(size_t bytes, const void *buffer) override;
    virtual void close() override;

private:
    PROCESS_INFORMATION proc_info;

    HANDLE pipe_stdin[2], pipe_stdout[2];
};

windows_process_rwops::windows_process_rwops(char *const args[]) {
    SECURITY_ATTRIBUTES attrs;

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

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin[PIPE_READ];
    start_info.hStdError = pipe_stdout[PIPE_WRITE];
    start_info.hStdOutput = pipe_stdout[PIPE_WRITE];
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    char cmdline[FILENAME_MAX];
    strcpy(cmdline, args[0]);
    strcat(cmdline, ".exe");
    for (char *const *cmd = args + 1; *cmd != nullptr; ++cmd) {
        strcat(cmdline, " \"");
        strcat(cmdline, *cmd);
        strcat(cmdline, "\"");
    }

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
    close();
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

void windows_process_rwops::close() {
    CloseHandle(pipe_stdout[PIPE_READ]);
    CloseHandle(pipe_stdin[PIPE_READ]);
}

std::unique_ptr<rwops> open_process(char *const args[]) {
    return std::make_unique<windows_process_rwops>(args);
}

#endif