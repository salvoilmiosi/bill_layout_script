#ifdef _WIN32

#include "proc_windows.h"

#include "utils.h"

void windows_pipe::init(SECURITY_ATTRIBUTES &attrs, int which) {
    close();
    if (!CreatePipe(&m_handles[PIPE_READ], &m_handles[PIPE_WRITE], &attrs, 0)
        || !SetHandleInformation(m_handles[which], HANDLE_FLAG_INHERIT, 0))
        throw process_error("Errore nella creazione della pipe");
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

windows_process::windows_process() : stream_out(pipe_stdout), stream_err(pipe_stderr), stream_in(pipe_stdin) {}

windows_process::windows_process(const char *args[]) : windows_process() {
    open(args);
}

void windows_process::open(const char *args[]) {
    close();
    
    SECURITY_ATTRIBUTES attrs;
    ZeroMemory(&attrs, sizeof(SECURITY_ATTRIBUTES));

    attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
    attrs.bInheritHandle = true;
    attrs.lpSecurityDescriptor = nullptr;

    pipe_stdout.init(attrs, PIPE_READ);
    pipe_stderr.init(attrs, PIPE_READ);
    pipe_stdin.init(attrs, PIPE_WRITE);

    STARTUPINFOA start_info;
    ZeroMemory(&start_info, sizeof(STARTUPINFOA));

    start_info.cb = sizeof(STARTUPINFOA);
    start_info.hStdInput = pipe_stdin.m_handles[PIPE_READ];
    start_info.hStdError = pipe_stderr.m_handles[PIPE_WRITE];
    start_info.hStdOutput = pipe_stdout.m_handles[PIPE_WRITE];
    start_info.dwFlags |= STARTF_USESTDHANDLES;

    std::string command;
    command.reserve(1024);

    std::string arg;
    while (true) {
        arg = *args++;

        bool quote;
        if (arg.empty()) {
            // we need to quote empty arguments, otherwise they'd just
            // disappear
            quote = true;
        } else { // non-empty
            // escape any quotes present in the string to avoid interfering
            // with the command line parsing in the child process
            string_replace(arg, "\"", "\\\"");

            // and quote any arguments containing the spaces to prevent them from
            // being broken down
            quote = arg.find_first_of(" \t") != std::string::npos;
        }

        if (quote)
            command += '\"' + arg + '\"';
        else
            command += arg;

        if (!*args)
            break;

        command += ' ';
    }

    PROCESS_INFORMATION proc_info;
    ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));

    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, true,
            CREATE_NO_WINDOW, nullptr, nullptr, &start_info, &proc_info)) {
        throw process_error("Could not open child process");
    } else {
        process = proc_info.hProcess;
        CloseHandle(proc_info.hThread);

        pipe_stdout.close(PIPE_WRITE);
        pipe_stderr.close(PIPE_WRITE);
        pipe_stdin.close(PIPE_READ);
    }
}

void windows_process::close() {
    if (process) {
        CloseHandle(process);
        process = nullptr;
    }
}

windows_process::~windows_process() {
    close();
}

int windows_process::wait_finished() {
    WaitForSingleObject(process, INFINITE);
    DWORD exit_code;
    if (!GetExitCodeProcess(process, &exit_code)) {
        throw process_error("Impossibile ottenere exit code");
    }
    return exit_code;
}

void windows_process::abort() {
    TerminateProcess(process, 1);
}

#endif