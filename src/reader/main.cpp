#include <iostream>

#include "result.h"
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
    result res;
    try {
        layout.openFile(file_bolletta);
    } catch (layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    }

    try {
        xpdf::pdf_info info = xpdf::pdf_get_info(app_dir, file_pdf);

        for (auto &box : layout.boxes) {
            std::string text = xpdf::pdf_to_text(app_dir, file_pdf, info, box);
            res.read_box(box, text);
        }
    } catch (pipe_error &error) {
        std::cerr << error.message << std::endl;
        return 2;
    }

    std::cout << res << std::endl;

    return 0;
}