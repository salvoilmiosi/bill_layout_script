#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

#include "bytecode.h"

#include <istream>

struct disassembler {
    std::vector<command_args> m_commands;
    std::vector<std::string> m_strings;

    bool read_bytecode(std::istream &input);
    const std::string &get_string(string_ref ref);
};

#endif