#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <fmt/core.h>

#include "reader.h"

int main(int argc, char **argv) {
    enum {
        FLAG_NONE,
        FLAG_PDF,
        FLAG_LAYOUT_DIR,
    } options_flag = FLAG_NONE;

    Json::Value result = Json::objectValue;
    result["error"] = false;

    std::filesystem::path input_file;
    std::filesystem::path file_pdf;
    std::filesystem::path layout_dir;
    bool exec_script = false;
    bool debug = false;

    auto output_error = [&](const std::string &message) {
        result["error"] = true;
        result["message"] = message;
        std::cout << result;
        exit(1);
    };

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
        output_error("Specificare il file pdf di input");
    } else if (!std::filesystem::exists(file_pdf)) {
        output_error(fmt::format("Impossibile aprire il file pdf {0}", file_pdf.string()));
    }

    reader m_reader;

    std::ifstream ifs;
    bool input_stdin = false;
    bool in_file_layout = true;

    if (input_file.empty()) {
        output_error("Specificare un file di input");
    } else if (input_file == "-") {
        input_stdin = true;
    } else {
        if (!std::filesystem::exists(input_file)) {
            output_error(fmt::format("Impossibile aprire il file layout {0}", input_file.string()));
        }
        ifs.open(input_file, std::ifstream::binary | std::ifstream::in);
        if (layout_dir.empty()) {
            layout_dir = input_file.parent_path();
        }
    }

    try {
        m_reader.open_pdf(file_pdf.string());

        if (exec_script) {
            if (input_stdin) {
                m_reader.exec_program(std::cin);
            } else {
                m_reader.exec_program(ifs);
                ifs.close();
            }

            in_file_layout = false;
        }
        if (!layout_dir.empty()) {
            const auto &layout_path = m_reader.get_global("layout");
            if (!layout_path.empty()) {
                input_file = layout_dir / layout_path.str();
                input_file.replace_extension(".out");
                if (!std::filesystem::exists(input_file)) {
                    output_error(fmt::format("Impossibile aprire il file layout {0}", input_file.string()));
                }
                ifs.open(input_file, std::ifstream::binary | std::ifstream::in);
                in_file_layout = true;
                input_stdin = false;
            }
        }
        if (in_file_layout) {
            if (input_stdin) {
                m_reader.exec_program(std::cin);
            } else {
                m_reader.exec_program(ifs);
                ifs.close();
            }
        }
    } catch (const layout_error &error) {
        output_error(error.message);
    }
#ifdef NDEBUG
    catch (...) {
        output_error("Errore sconosciuto");
    }
#endif

    m_reader.save_output(result, debug);
    std::cout << result;

    return 0;
}