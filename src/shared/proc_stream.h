#ifndef __PROC_STREAM_H__
#define __PROC_STREAM_H__

#include <streambuf>
#include <iostream>
#include "subprocess.h"

class proc_istreambuf : public std::streambuf {
public:
    proc_istreambuf(subprocess &proc) : proc(proc) {}

protected:
    virtual int underflow() override {
        int nbytes = proc.read_stdout(BUFSIZE, buffer);
        if (nbytes <= 0) {
            return std::char_traits<char>::eof();
        } else {
            setg(buffer, buffer, buffer + nbytes);
            return std::char_traits<char>::to_int_type(*gptr());
        }
    };

private:
    subprocess &proc;
    char buffer[BUFSIZE];
};

class proc_istream : public std::istream {
public:
    proc_istream(subprocess &proc) : std::istream(nullptr), buffer(proc) {
        std::ios::init(&buffer);
    }

private:
    proc_istreambuf buffer;
};

class proc_ostreambuf : public std::streambuf {
public:
    proc_ostreambuf(subprocess &proc) : proc(proc) {}

protected:
    virtual std::streamsize xsputn(const char_type *s, std::streamsize n) override {
        return proc.write_stdin(n, s);
    }

private:
    subprocess &proc;
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