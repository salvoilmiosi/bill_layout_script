#include <iostream>
#include <filesystem>

#include "parser.h"
#include "bytecode_printer.h"

using namespace bls;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << intl::translate("REQUIRED_INPUT_BLS") << std::endl;
        return 1;
    }
    try {
        auto code = parser{}(layout_box_list(argv[1]));
        for (auto it = code.begin(); it != code.end(); ++it) {
            std::cout << bytecode_printer(code, it) << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << intl::translate("UNKNOWN_ERROR") << std::endl;
        return 1;
    }
    return 0;
}