#ifndef __BINARY_IO_H__
#define __BINARY_IO_H__

#include "bytecode.h"
#include <iostream>

constexpr bool is_big_endian() {
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

template <typename T>
T swap_endian(T u) {
    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

template<typename T>
void readData(std::istream &input, T &buffer) {
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
    if (!is_big_endian()) {
        buffer = swap_endian(buffer);
    }
}

template<> void readData(std::istream &input, std::string &buffer) {
    string_size len;
    readData(input, len);
    buffer.resize(len, '\0');
    input.read(buffer.data(), len);
}

template<typename T> T readData(std::istream &input) {
    T buffer;
    readData(input, buffer);
    return buffer;
}

template<typename T>
void writeData(std::ostream &output, const T &data) {
    if (is_big_endian()) {
        output.write(reinterpret_cast<const char *>(&data), sizeof(data));
    } else {
        T swapped = swap_endian(data);
        output.write(reinterpret_cast<const char *>(&swapped), sizeof(data));
    }
}

template<> void writeData<std::string>(std::ostream &output, const std::string &str) {
    writeData<string_size>(output, str.size());
    output.write(str.c_str(), str.size());
}

#endif