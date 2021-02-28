#ifndef __BINARY_BLS_H__
#define __BINARY_BLS_H__

#include "bytecode.h"

#include <filesystem>

namespace binary_bls {
    bytecode read(const std::filesystem::path &filename);
    
    void write(const bytecode &code, const std::filesystem::path &filename);
}

#endif