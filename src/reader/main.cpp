#include <iostream>
#include <fstream>
#include <filesystem>

#include "parser.h"
#include "../shared/xpdf.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Utilizzo: layout_reader file.pdf file.bls/[file.txt -f dir_layout]" << std::endl;
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

    bool debug = false;
    bool only_script = false;
    bool find_file_layout = false;
    bool read_simple = false;
    std::string layout_dir;

    if (argc >= 4 && argv[3][0] == '-') {
        for (char *c = argv[3] + 1; *c!='\0'; ++c) {
            switch(*c) {
            case 'i':
                read_simple = true;
                break;
            case 'd':
                debug = true;
                break;
            case 's':
                only_script = true;
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

    std::unique_ptr<std::ifstream> ifs;
    
    if (file_layout != "-") {
        ifs = std::make_unique<std::ifstream>(file_layout);
        if (ifs->bad()) {
            std::cerr << "Impossibile aprire il file " << file_layout << " in input" << std::endl;
            return 1;
        }
    }

    try {
        if (only_script) {
            std::string text = pdf_whole_file_to_text(file_pdf, read_simple ? MODE_SIMPLE : MODE_RAW);
            if (ifs) {
                result.read_script(*ifs, text);
                ifs->close();
            } else {
                result.read_script(std::cin, text);
            }
        } else {
            if (find_file_layout) {
                std::string text = pdf_whole_file_to_text(file_pdf, read_simple ? MODE_SIMPLE : MODE_RAW);
                if (ifs) {
                    result.read_script(*ifs, text);
                    ifs->close();
                } else {
                    result.read_script(std::cin, text);
                }
                file_layout = layout_dir + '/' + result.get_variable("layout").str();
                ifs = std::make_unique<std::ifstream>(file_layout);
                if (ifs->bad()) {
                    std::cerr << "Impossibile aprire il file " << file_layout << " in input" << std::endl;
                    return 1;
                }
            }
            bill_layout_script layout;
            if (ifs) {
                *ifs >> layout;
                ifs->close();
            } else {
                std::cin >> layout;
            }

            pdf_info info = pdf_get_info(file_pdf);

            for (auto &box : layout.boxes) {
                result.read_box(file_pdf, info, box);
            }
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

    std::cout << result << std::endl;

    return 0;
}