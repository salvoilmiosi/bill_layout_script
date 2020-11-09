#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "parser.h"
#include "assembler.h"

int main(int argc, char **argv) {
    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    parser result;
    std::filesystem::path input_file;
    std::filesystem::path output_file;

    std::unique_ptr<std::ifstream> ifs;

    if (argc >= 3) {
        input_file = argv[1];
        output_file = argv[2];
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

        std::ofstream ofs(output_file, std::ofstream::binary | std::ofstream::out);
        for (auto &line : result.get_output_asm()) {
            std::cout << line;
            if (line.back() == ':') {
                std::cout << ' ';
            } else {
                std::cout << std::endl;
            }
        }
        assembler(result.get_output_asm()).save_output(ofs);
        ofs.close();
    } catch (const layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const assembly_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const std::exception &error) {
        std::cerr << error.what();
        return 1;
    }

    return 0;
}