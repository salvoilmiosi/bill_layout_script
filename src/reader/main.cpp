#include <iostream>
#include <fstream>
#include <filesystem>

#include "parser.h"
#include "../shared/xpdf.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cout << "Utilizzo: layout_reader file.pdf file.bls [-options]" << std::endl;
        return 0;
    }

    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    std::string file_pdf = argv[1];
    std::string file_layout = argv[2];

    if (!std::filesystem::exists(file_pdf)) {
        std::cerr << "Il file " << file_pdf << " non esiste";
        return 1;
    }

    parser result;
    bool whole_file_script = false;

    if (argc >= 4 && argv[3][0] == '-') {
        for (char *c = argv[3] + 1; *c!='\0'; ++c) {
            switch(*c) {
            case 'd':
                result.debug = true;
                break;
            case 'w':
                whole_file_script = true;
                break;
            default:
                std::cerr << "Opzione sconosciuta: " << *c << std::endl;
            }
        }
    }

    std::istream *in = &std::cin;
    
    if (file_layout != "-") {
        in = new std::ifstream(file_layout);
        if (in->bad()) {
            std::cerr << "Impossibile aprire il file " << file_layout << " in input" << std::endl;
            return 1;
        }
    }

    try {
        if (whole_file_script) {
            std::string text = xpdf::pdf_whole_file_to_text(app_dir, file_pdf);
            result.read_script(*in, text);
        } else {
            layout_bolletta layout;
            *in >> layout;

            xpdf::pdf_info info = xpdf::pdf_get_info(app_dir, file_pdf);

            for (auto &box : layout.boxes) {
                std::string text = xpdf::pdf_to_text(app_dir, file_pdf, info, box);
                result.read_box(box, text);
            }
        }
    } catch (layout_error &error) {
        std::cerr << error.message << std::endl;
        return 1;
    } catch (pipe_error &error) {
        std::cerr << error.message << std::endl;
        return 2;
    } catch (parsing_error &error) {
        std::cerr << error.message << '\n' << error.line << std::endl;
        return 3;
    }
        
    if (auto *ifs = dynamic_cast<std::ifstream *>(in)) {
        ifs->close();
        delete ifs;
    }

    std::cout << result << std::endl;

    return 0;
}