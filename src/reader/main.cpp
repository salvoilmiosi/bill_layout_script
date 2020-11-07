#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "reader.h"
#include "../shared/xpdf.h"

int main(int argc, char **argv) {
    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    enum {
        FLAG_NONE,
        FLAG_PDF,
        FLAG_LAYOUT_DIR,
    } options_flag = FLAG_NONE;

    reader result;
    std::filesystem::path input_file;
    std::filesystem::path file_pdf;
    std::filesystem::path layout_dir;
    bool exec_script = false;
    bool in_file_layout = true;
    bool debug = false;

    for (++argv; argc > 1; --argc, ++argv) {
        switch (options_flag) {
        case FLAG_NONE:
            if (strcmp(*argv, "-p") == 0) options_flag = FLAG_PDF;
            else if (strcmp(*argv, "-l") == 0) options_flag = FLAG_LAYOUT_DIR;
            else if (strcmp(*argv, "-s") == 0) exec_script = true;
            else if (strcmp(*argv, "-d") == 0) debug = true;
            else input_file = *argv;
            break;
        case FLAG_PDF:
            file_pdf = *argv;
            options_flag = FLAG_NONE;
            break;
        case FLAG_LAYOUT_DIR:
            layout_dir = *argv;
            options_flag = FLAG_NONE;
            break;
        }
    }

    if (file_pdf.empty()) {
        std::cerr << "Specificare il file pdf di input" << std::endl;
        return 1;
    } else if (!std::filesystem::exists(file_pdf)) {
        std::cerr << "Impossibile aprire il file pdf " << file_pdf << std::endl;
        return 1;
    }

    std::unique_ptr<std::ifstream> ifs;

    if (input_file.empty()) {
        std::cerr << "Specificare un file di input, o - per stdin" << std::endl;
        return 1;
    } else if (input_file != "-") {
        if (!std::filesystem::exists(input_file)) {
            std::cerr << "Impossibile aprire il file layout " << input_file << std::endl;
            return 1;
        }
        ifs = std::make_unique<std::ifstream>(input_file);
        if (layout_dir.empty()) {
            layout_dir = input_file.parent_path();
        }
    }

    try {
        pdf_info info = pdf_get_info(file_pdf.string());
        if (exec_script) {
            bill_layout_script layout;
            if (ifs) {
                *ifs >> layout;
                ifs->close();
            } else {
                std::cin >> layout;
            }

            result.read_layout(info, layout);
            in_file_layout = false;
        }
        if (!layout_dir.empty()) {
            const auto &layout_path = result.get_global("layout");
            if (!layout_path.empty()) {
                input_file = layout_dir / layout_path.str();
                if (!std::filesystem::exists(input_file)) {
                    std::cerr << "Impossibile aprire il file layout " << input_file << std::endl;
                    return 1;
                }
                ifs = std::make_unique<std::ifstream>(input_file);
                in_file_layout = true;
            }
        }
        if (in_file_layout) {
            bill_layout_script layout;
            if (ifs) {
                *ifs >> layout;
                ifs->close();
            } else {
                std::cin >> layout;
            }

            result.read_layout(info, layout);
        }
    } catch (const layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (const xpdf_error &error) {
        std::cerr << error.message << std::endl;
        return 2;
    } catch (const parsing_error &error) {
        std::cerr << error.message << ": " << error.line << std::endl;
        return 3;
    } catch (const std::exception &error) {
        std::cerr << error.what();
        return 4;
    }

    result.print_output(std::cout, debug);

    return 0;
}