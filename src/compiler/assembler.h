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

class assembler {
public:
    void read_lines(const std::vector<std::string> &lines);
    void write_bytecode(std::ostream &output);

private:
    std::vector<command_args> out_lines;
    std::vector<std::string> out_strings;

    template<typename ... Ts> command_args add_command(const Ts & ... args) {
        return out_lines.emplace_back(args ...);
    }

    string_ref add_string(const std::string &str);
};

#endif