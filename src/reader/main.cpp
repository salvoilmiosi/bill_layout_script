#include <iostream>
#include <sstream>

#include "../shared/layout.h"
#include "../shared/xpdf.h"

std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    str.erase(0, str.find_first_not_of(chars));
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

void parse_values(const layout_box &box, const std::string &text) {
    auto print_value = [](const std::string &name, const std::string &value) {
        if (name.substr(0, 4) == "skip") {
            return;
        }
        std::cout << name << " = " << value << std::endl;
    };
    std::istringstream iss_names(box.parse_string);
    std::istringstream iss_values(text);
    std::string value;
    std::vector<std::string> names;
    while(!iss_names.eof()) {
        std::string name;
        iss_names >> name;
        names.push_back(name);
    }
    switch(box.type) {
        case BOX_SINGLE:
            while(true) {
                std::string token;
                iss_values >> token;
                if (iss_values.eof()) break;
                value.append(token + " ");
            }
            trim(value);
            print_value(names[0], value);
            break;
        case BOX_MULTIPLE:
        {
            for (size_t i=0; i<names.size(); ++i) {
                iss_values >> value;
                if (iss_values.eof()) break;
                print_value(names[i], value);
            }
            break;
        }
        case BOX_HGRID:
            for (size_t i=0; ;++i) {
                iss_values >> value;
                if (iss_values.eof()) break;
                print_value (names[i % names.size()] + "[" + std::to_string(i / names.size()) + "]", value);
            }
            break;
        case BOX_VGRID:
            for (size_t i=0; ;++i) {
                iss_values >> value;
                if (iss_values.eof()) break;
                print_value (names[i / names.size()] + "[" + std::to_string(i % names.size()) + "]", value);
            }
            break;
    }
}

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
            parse_values(box, text);
        }
    } catch (pipe_error &error) {
        std::cerr << error.message << std::endl;
        return 2;
    }

    return 0;
}