#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#include <vector>
#include <string>
#include <ostream>

#include "bytecode.h"

struct assembly_error {
    std::string message;

    assembly_error(const std::string &message) : message(message) {}
};

bytecode read_lines(const std::vector<std::string> &lines);

#endif