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

class assembler : public bytecode {
public:
    void read_lines(const std::vector<std::string> &lines);

private:
    template<typename ... Ts> command_args add_command(const Ts & ... args) {
        return m_commands.emplace_back(args ...);
    }
};

#endif