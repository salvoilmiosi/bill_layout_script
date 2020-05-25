#include "pdf_to_image.h"

#include <filesystem>
#include <iostream>

#include "../shared/pipe.h"

wxImage pdf_to_image(const std::string &pdf, int page) {
    if (!wxFileExists(pdf)) {
        throw xpdf_error(std::string("File \"") + pdf + "\" does not exist");
    }
    
    char page_str[16];
    snprintf(page_str, 16, "%d", page);

    const char *args[] = {
        "pdftopng",
        "-f", page_str,  "-l", page_str,
        pdf.c_str(), "temp",
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::cout << output << std::endl;

    char filename[32];
    snprintf(filename, 32, "temp-%06d.png", page);

    wxImage img;

    if (wxFileExists(filename)) {
        img.LoadFile(filename, wxBITMAP_TYPE_PNG);
        wxRemoveFile(filename);
        
        if (img.IsOk()) {
            return img;
        }
    }
    throw xpdf_error("Could not open image");
}