#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "parser.h"
#include "assembler.h"

int main(int argc, char **argv) {
    parser result;
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    bool quiet = false;
    bool debug = false;

    std::unique_ptr<std::ifstream> ifs;
    
    enum {
        FLAG_NONE,
        FLAG_OUTPUT,
    } options_flag = FLAG_NONE;

    for (++argv; argc > 1; --argc, ++argv) {
        switch (options_flag) {
        case FLAG_NONE:
            if (strcmp(*argv, "-o") == 0) options_flag = FLAG_OUTPUT;
            else if (strcmp(*argv, "-q") == 0) quiet = true;
            else if (strcmp(*argv, "-d") == 0) debug = true;
            else input_file = *argv;
            break;
        case FLAG_OUTPUT:
            output_file = *argv;
            options_flag = FLAG_NONE;
            break;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Specificare un file di input, o - per stdin" << std::endl;
        return 1;
    } else if (input_file != "-") {
        if (!std::filesystem::exists(input_file)) {
            std::cerr << "Impossibile aprire il file layout " << input_file << std::endl;
            return 1;
        }
        ifs = std::make_unique<std::ifstream>(input_file);

        if (output_file.empty()) {
            output_file = input_file;
            output_file.replace_extension("out");
        }
    }

    if (!debug && output_file.empty()) {
        std::cerr << "Specificare un file di output" << std::endl;
        return 1;
    }

    try {
        bill_layout_script layout;
        if (ifs) {
            *ifs >> layout;
            ifs->close();
        } else {
            std::cin >> layout;
        }

        result.read_layout(layout);

        if (!quiet) {
            for (auto &line : result.get_output_asm()) {
                std::cout << line;
                if (line.back() == ':') {
                    std::cout << ' ';
                } else {
                    std::cout << std::endl;
                }
            }
        }

        if (!debug) {
            std::ofstream ofs(output_file, std::ofstream::binary | std::ofstream::out);
            assembler(result.get_output_asm()).save_output(ofs);
            ofs.close();
        }
    } catch (const layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const assembly_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    }

    return 0;
}