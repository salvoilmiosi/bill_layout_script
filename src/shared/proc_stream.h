#ifndef __PROC_STREAM_H__
#define __PROC_STREAM_H__

#include <streambuf>
#include <iostream>
#include "subprocess.h"

class proc_istreambuf : public std::streambuf {
private:
    proc_istreambuf(subprocess &proc, bool is_stderr = false) : proc(proc), is_stderr(is_stderr) {}

protected:
    virtual int underflow() override {
        int nbytes;
        if (is_stderr) {
            nbytes = proc.read_stderr(BUFSIZE, buffer);
        } else {
            nbytes = proc.read_stdout(BUFSIZE, buffer);
        }
        if (nbytes <= 0) {
            return std::char_traits<char>::eof();
        } else {
            setg(buffer, buffer, buffer + nbytes);
            return std::char_traits<char>::to_int_type(*gptr());
        }
    };

private:
    subprocess &proc;
    bool is_stderr;
    char buffer[BUFSIZE];

    friend class proc_istream;
    friend class proc_err_istream;
};

class proc_istream : public std::istream {
public:
    proc_istream(subprocess &proc) : std::istream(nullptr), buffer(proc) {
        std::ios::init(&buffer);
    }

private:
    proc_istreambuf buffer;
};

class proc_err_istream : public std::istream {
public:
    proc_err_istream(subprocess &proc) : std::istream(nullptr), buffer(proc, true) {
        std::ios::init(&buffer);
    }

private:
    proc_istreambuf buffer;
};

class proc_ostreambuf : public std::streambuf {
private:
    proc_ostreambuf(subprocess &proc) : proc(proc) {}

protected:
    virtual int overflow(int ch) override {
        if (proc.write_stdin(1, &ch) == 1) {
            return ch;
        } else {
            return std::char_traits<char>::eof();
        }
    }

    virtual std::streamsize xsputn(const char_type *s, std::streamsize n) override {
        return proc.write_stdin(n, s);
    }

private:
    subprocess &proc;

    friend class proc_ostream;
};

class proc_ostream : public std::ostream {
public:
    proc_ostream(subprocess &proc) : std::ostream(nullptr), buffer(proc) {
        std::ios::init(&buffer);
    }

private:
    proc_ostreambuf buffer;
};

#endif