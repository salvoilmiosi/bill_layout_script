#include <iostream>
#include <fstream>

#include "parser.h"
#include "../shared/xpdf.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Utilizzo: layout_reader file.pdf file.bolletta" << std::endl;
        return 0;
    }

    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));
    const char *file_pdf = argv[1];
    const char *file_bolletta = argv[2];

    layout_bolletta layout;
    parser result;

    if (argc >= 4 && strcmp(argv[3],"-d") == 0) {
        result.debug = true;
    }

    try {
        if (strcmp(file_bolletta,"-")==0) {
            std::cin >> layout;
        } else {
            std::ifstream ifs(file_bolletta);
            ifs >> layout;
            ifs.close();
        }
    } catch (layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    }

    try {
        xpdf::pdf_info info = xpdf::pdf_get_info(app_dir, file_pdf);

        for (auto &box : layout.boxes) {
            std::string text = xpdf::pdf_to_text(app_dir, file_pdf, info, box);
            result.read_box(box, text);
        }
    } catch (pipe_error &error) {
        std::cerr << error.message << std::endl;
        return 2;
    } catch (parsing_error &error) {
        std::cerr << error.message << '\n' << error.line << std::endl;
        return 3;
    }

    std::cout << result << std::endl;

    return 0;
}