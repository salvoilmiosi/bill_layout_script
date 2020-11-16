#include "pdf_to_image.h"

#include <filesystem>
#include <iostream>

#include "../shared/pipe.h"

#define PDFTOCAIRO_BIN "pdftocairo"

wxImage pdf_to_image(const std::string &pdf, int page) {
    if (!wxFileExists(pdf)) {
        throw xpdf_error(std::string("File \"") + pdf + "\" does not exist");
    }
    
    auto page_str = std::to_string(page);

    const char *args[] = {
        PDFTOCAIRO_BIN,
        "-f", page_str.c_str(),  "-l", page_str.c_str(),
        "-png", "-singlefile",
        pdf.c_str(), "temp",
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::cout << output << std::endl;

    wxImage img;

    if (wxFileExists("temp.png")) {
        img.LoadFile("temp.png", wxBITMAP_TYPE_PNG);
        wxRemoveFile("temp.png");
        
        if (img.IsOk()) {
            return img;
        }
    }
    throw xpdf_error("Could not open image");
}