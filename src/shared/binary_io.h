#ifndef __BINARY_IO_H__
#define __BINARY_IO_H__

#include <fstream>
#include <string>

struct binary_ofstream : public std::ofstream {
    binary_ofstream(const std::string &filename) : std::ofstream(filename, std::ios::binary | std::ios::out) {}

    void writeData(const void *data, int length);

    void writeByte(uint8_t num);
    void writeShort(uint16_t num);
    void writeInt(uint32_t num);
    void writeLong(uint64_t num);
    void writeFloat(float num);
    void writeDouble(double num);
    void writeString(const std::string &str);
};

struct binary_ifstream : public std::ifstream {
    binary_ifstream(const std::string &filename) : std::ifstream(filename, std::ios::binary | std::ios::in) {}

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