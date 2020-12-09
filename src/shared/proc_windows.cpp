#if defined(WIN32) || defined(_WIN32)

#include "proc_windows.h"

void windows_pipe::create_pipe(SECURITY_ATTRIBUTES &attrs, int which) {
    if (!CreatePipe(&m_handles[PIPE_READ], &m_handles[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(m_handles[which], HANDLE_FLAG_INHERIT, 0))
        throw process_error("Error creating pipe");
}

int windows_pipe::read(size_t bytes, void *buffer) {
    DWORD bytes_read;
    if (!ReadFile(m_handles[PIPE_READ], buffer, bytes, &bytes_read, nullptr))
        return -1;
    return bytes_read;
}

int windows_pipe::write(size_t bytes, const void *buffer) {
    DWORD bytes_written;
    if (!WriteFile(m_handles[PIPE_WRITE], buffer, bytes, &bytes_written, nullptr))
        return -1;
    return bytes_written;
}

void windows_pipe::close(int which) {
    if (which != PIPE_WRITE && m_handles[PIPE_READ]) {
        CloseHandle(m_handles[PIPE_READ]);
        m_handles[PIPE_READ] = nullptr;
    }
    if (which != PIPE_READ && m_handles[PIPE_WRITE]) {
        CloseHandle(m_handles[PIPE_WRITE]);
        m_handles[PIPE_WRITE] = nullptr;
    }
}

windows_process::windows_process(const char *args[]) {
    SECURITY_ATTRIBUTES attrs;
    ZeroMemory(&attrs, sizeof(SECURITY_ATTRIBUTES));

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    pipe_stdout.create_pipe(attrs, PIPE_READ);
    pipe_stderr.create_pipe(attrs, PIPE_READ);
    pipe_stdin.create_pipe(attrs, PIPE_WRITE);

    STARTUPINFOA start_info;
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin.m_handles[PIPE_READ];
    start_info.hStdError = pipe_stderr.m_handles[PIPE_WRITE];
    start_info.hStdOutput = pipe_stdout.m_handles[PIPE_WRITE];
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

        pipe_stdout.close(PIPE_WRITE);
        pipe_stderr.close(PIPE_WRITE);
        pipe_stdin.close(PIPE_READ);

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

#endif