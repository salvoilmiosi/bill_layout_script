#include <iostream>
#include <fstream>
#include <filesystem>

#include "parser.h"
#include "../shared/xpdf.h"

int main(int argc, char **argv) {
    std::string app_dir = argv[0];
    app_dir = app_dir.substr(0, app_dir.find_last_of("\\/"));

    std::string file_pdf = argv[1];
    std::string file_layout = argv[2];

    if (!std::filesystem::exists(file_pdf)) {
        std::cerr << "Il file " << file_pdf << " non esiste";
        return 1;
    }

    bool debug = false;
    bool find_file_layout = false;
    std::string layout_dir;

    if (argc >= 4 && argv[3][0] == '-') {
        for (char *c = argv[3] + 1; *c!='\0'; ++c) {
            switch(*c) {
            case 'd':
                debug = true;
                break;
            case 'f':
                find_file_layout = true;
                if (argc >= 5) {
                    layout_dir = argv[4];
                } else {
                    std::cerr << "-f richiede directory dei file di layout" << std::endl;
                    return 1;
                }
                break;
            default:
                std::cerr << "Opzione sconosciuta: " << *c << std::endl;
                return 1;
            }
        }
    }

    parser result;
    result.debug = debug;
    std::istream *in = &std::cin;
    
    if (file_layout != "-") {
        in = new std::ifstream(file_layout);
        if (in->bad()) {
            std::cerr << "Impossibile aprire il file " << file_layout << " in input" << std::endl;
            delete in;
            return 1;
        }
    }

    try {
        if (find_file_layout) {
            std::string text = pdf_whole_file_to_text(app_dir, file_pdf);
            result.read_script(*in, text);
            if (auto *ifs = dynamic_cast<std::ifstream *>(in)) {
                ifs->close();
                delete ifs;
            }
            file_layout = layout_dir + '/' + result.get_file_layout();
            in = new std::ifstream(file_layout);
            if (in->bad()) {
                std::cerr << "Impossibile aprire il file " << file_layout << " in input" << std::endl;
                delete in;
                return 1;
            }
        }
        layout_bolletta layout;
        *in >> layout;

        pdf_info info = pdf_get_info(app_dir, file_pdf);

        for (auto &box : layout.boxes) {
            result.read_box(app_dir, file_pdf, info, box);
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
    }
    
    if (auto *ifs = dynamic_cast<std::ifstream *>(in)) {
        ifs->close();
        delete ifs;
    }

    std::cout << result << std::endl;

    return 0;
}