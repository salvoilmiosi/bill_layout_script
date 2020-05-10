#ifndef __BINARY_IO_H__
#define __BINARY_IO_H__

#include <fstream>
#include <string>

struct rwops {
    virtual int read(size_t bytes, void *buffer) { return 0; }
    virtual int write(size_t bytes, const void *buffer) { return 0; }
    virtual void close() { }

    std::string read_all();
    int write_all(const std::string &buffer);
};

struct istream_rwops : public rwops {
    std::istream &stream;

    istream_rwops(std::istream &stream) : stream(stream) {}

    int read(size_t bytes, void *buffer) override {
        stream.read(static_cast<char *>(buffer), bytes);
        return bytes;
    }
};

struct ostream_rwops : public rwops {
    std::ostream &stream;

    ostream_rwops(std::ostream &stream) : stream(stream) {}

    int write(size_t bytes, const void *buffer) override {
        stream.write(static_cast<const char *>(buffer), bytes);
        return bytes;
    }
};

struct binary_io {
    rwops &ops;

    binary_io(rwops &ops) : ops(ops) {}

    void writeData(const void *data, int length);

    void writeByte(uint8_t num);
    void writeShort(uint16_t num);
    void writeInt(uint32_t num);
    void writeLong(uint64_t num);
    void writeFloat(float num);
    void writeDouble(double num);
    void writeString(const std::string &str);

    void readData(void *out, int length);

    uint8_t readByte();
    uint16_t readShort();
    uint32_t readInt();
    uint64_t readLong();
    float readFloat();
    double readDouble();
    std::string readString();
};

#endif