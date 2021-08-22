#include <iostream>
#include <filesystem>

#include "parser.h"
#include "bytecode_printer.h"

using namespace bls;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << intl::format("REQUIRED_INPUT_BLS") << std::endl;
        return 1;
    }
    try {
        std::filesystem::path input_file = argv[1];
        
        auto code = parser{}.read_layout(input_file, layout_box_list::from_file(input_file));
        for (auto it = code.begin(); it != code.end(); ++it) {
            std::cout << bytecode_printer(code, it) << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << intl::format("UNKNOWN_ERROR") << std::endl;
        return 1;
    }
    return 0;
}