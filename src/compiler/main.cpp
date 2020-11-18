#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "parser.h"
#include "assembler.h"

int main(int argc, char **argv) {
    std::filesystem::path input_file;
    std::filesystem::path output_file;

    bool debug = false;
    bool read_asm = false;

    std::ifstream ifs;
    
    enum {
        FLAG_NONE,
        FLAG_OUTPUT,
    } options_flag = FLAG_NONE;

    for (++argv; argc > 1; --argc, ++argv) {
        switch (options_flag) {
        case FLAG_NONE:
            if (strcmp(*argv, "-o") == 0) options_flag = FLAG_OUTPUT;
            else if (strcmp(*argv, "-d") == 0) debug = true;
            else if (strcmp(*argv, "-s") == 0) read_asm = true;
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
        ifs.open(input_file);

        if (output_file.empty()) {
            output_file = input_file;
            output_file.replace_extension("out");
        }
    }

    if (!debug && output_file.empty()) {
        std::cerr << "Specificare un file di output" << std::endl;
        return 1;
    }

    assembler m_asm;

    try {
        if (!read_asm) {
            bill_layout_script layout;
            if (ifs.is_open()) {
                ifs >> layout;
                ifs.close();
            } else {
                std::cin >> layout;
            }

            parser my_parser;
            my_parser.read_layout(layout);

            if (debug) {
                for (auto &line : my_parser.get_output_asm()) {
                    std::cout << line << std::endl;
                }
            }
            
            m_asm.read_lines(my_parser.get_output_asm());
        } else {
            std::vector<std::string> lines;
            std::string line;
            while(std::getline(ifs.is_open() ? ifs : std::cin, line)) {
                lines.push_back(line);
            }
            
            m_asm.read_lines(lines);
        }
    } catch (const layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const assembly_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    }
    
    if (!output_file.empty()) {
        if (output_file == "-") {
            m_asm.write_bytecode(std::cout);
        } else {
            std::ofstream ofs(output_file, std::ofstream::binary | std::ofstream::out);
            m_asm.write_bytecode(ofs);
        }
    }

    return 0;
}