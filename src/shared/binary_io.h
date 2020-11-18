#ifndef __BINARY_IO_H__
#define __BINARY_IO_H__

#include "bytecode.h"
#include <iostream>

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define BIG_ENDIAN
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define LITTLE_ENDIAN
#else
    #error "Cannot determine endianness"
#endif

template <typename T>
T swap_endian(T u) {
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

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
T readData(std::istream &input) {
    T buffer;
    input.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
#ifdef BIG_ENDIAN
    return buffer;
#else
    return swap_endian(buffer);
#endif
}

template<> std::string readData(std::istream &input) {
    string_size len = readData<string_size>(input);
    std::string buffer(len, '\0');
    input.read(buffer.data(), len);
    return buffer;
}

template<typename T>
void writeData(std::ostream &output, const T &data) {
#ifdef BIG_ENDIAN
    output.write(reinterpret_cast<const char *>(&data), sizeof(data));
#else
    T swapped = swap_endian(data);
    output.write(reinterpret_cast<const char *>(&swapped), sizeof(data));
#endif
}

template<> void writeData<std::string>(std::ostream &output, const std::string &str) {
    writeData<string_size>(output, str.size());
    output.write(str.c_str(), str.size());
}

#endif