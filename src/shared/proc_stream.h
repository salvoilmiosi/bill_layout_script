#ifndef __PROC_STREAM_H__
#define __PROC_STREAM_H__

#include <streambuf>
#include <iostream>
#include "subprocess.h"

class proc_streambuf : public std::streambuf {
public:
    proc_streambuf(subprocess &proc) : proc(proc) {}

protected:
    virtual std::streambuf *setbuf(char *, std::streamsize) override {
        return nullptr;
    }

    virtual std::streamsize xsgetn(char *s, std::streamsize n) override {
        return proc.read_stdout(n, s);
    };

    virtual std::streamsize xsputn(const char *s, std::streamsize n) override {
        return proc.write_stdin(n, s);
    };

private:
    subprocess &proc;
};

class proc_istream : public std::istream {
public:
    proc_istream(subprocess &proc) : std::istream(nullptr), buffer(proc) {
        std::ios::init(&buffer);
    }

private:
    proc_streambuf buffer;
};

class proc_ostream : public std::ostream {
public:
    proc_ostream(subprocess &proc) : std::ostream(nullptr), buffer(proc) {
        std::ios::init(&buffer);
    }

private:
    proc_streambuf buffer;
};

#endif