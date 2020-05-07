#include "pipe.h"

#include <windows.h>
#include <iostream>

class windows_process_rwops : public rwops {
public:
    windows_process_rwops(const std::string &cmd, const std::string &args, const std::string &cwd);
    ~windows_process_rwops();

public:
    virtual int read(size_t bytes, void *buffer) override;
    virtual int write(size_t bytes, const void *buffer) override;
    virtual void close() override;

private:
    PROCESS_INFORMATION proc_info;

    HANDLE child_pipe_read, child_pipe_write;
    HANDLE parent_pipe_read, parent_pipe_write;
};

windows_process_rwops::windows_process_rwops(const std::string &cmd, const std::string &args, const std::string &cwd) {
    SECURITY_ATTRIBUTES attrs;

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&child_pipe_read, &child_pipe_write, &attrs, 0)) {
        throw pipe_error("Error creating child pipe");
    }

    if (!SetHandleInformation(child_pipe_read, HANDLE_FLAG_INHERIT, 0)) {
        throw pipe_error("Error setting child pipe handle information");
    }

    if (!CreatePipe(&parent_pipe_read, &parent_pipe_write, &attrs, 0)) {
        throw pipe_error("Error creating parent pipe");
    }

    if (!SetHandleInformation(parent_pipe_write, HANDLE_FLAG_INHERIT, 0)) {
        throw pipe_error("Error setting parent pipe handle information");
    }

    STARTUPINFOA start_info;

    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = parent_pipe_read;
    start_info.hStdError = child_pipe_write;
    start_info.hStdOutput = child_pipe_write;
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::string cmdline = cmd + " " + args;

    if (!CreateProcessA(nullptr, const_cast<LPSTR>(cmdline.c_str()), nullptr, nullptr, true,
            CREATE_NO_WINDOW, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &start_info, &proc_info)) {
        throw pipe_error("Could not open child process");
    } else {
        CloseHandle(proc_info.hProcess);
        CloseHandle(proc_info.hThread);
        CloseHandle(child_pipe_write);
        CloseHandle(parent_pipe_read);
    }
}

windows_process_rwops::~windows_process_rwops() {
    close();
}

int windows_process_rwops::read(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(child_pipe_read, buffer, bytes, &bytes_read, nullptr))
        return 0;
    return bytes_read;
}

int windows_process_rwops::write(size_t bytes, const void *buffer) {
    DWORD bytes_written;
    if (!WriteFile(parent_pipe_write, buffer, bytes, &bytes_written, nullptr))
        return 0;
    return bytes_written;
}

void windows_process_rwops::close() {
    CloseHandle(child_pipe_read);
    CloseHandle(parent_pipe_write);
}

std::unique_ptr<rwops> open_process(const std::string &cmd, const std::string &args, const std::string &cwd) {
    return std::make_unique<windows_process_rwops>(cmd, args, cwd);
}

constexpr size_t BUFSIZE = 4096;

std::string rwops::read_all() {
    std::string out;

    char buffer[BUFSIZE];

    while (true) {
        size_t bytes_read = read(BUFSIZE, buffer);
        if (bytes_read <= 0) break;

        out.append(buffer, bytes_read);
    }

    return out;
}

int rwops::write_all(const std::string &buffer) {
    size_t bytes_written = 0;

    auto it_begin = buffer.begin();
    while (it_begin != buffer.end()) {
        auto it_end = it_begin + BUFSIZE;
        if (it_end > buffer.end()) {
            it_end = buffer.end();
        }

        bytes_written += write(it_end - it_begin, &(*it_begin));

        it_begin = it_end;
    }

    return bytes_written;
}