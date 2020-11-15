#include "pdf_to_image.h"

#include <filesystem>
#include <iostream>

#include "../shared/pipe.h"

#ifdef __linux__
#define PDFTOPNG_BIN "/usr/local/bin/pdftopng"
#else
#define PDFTOPNG_BIN "pdftopng"
#endif

wxImage pdf_to_image(const std::string &pdf, int page) {
    if (!wxFileExists(pdf)) {
        throw xpdf_error(std::string("File \"") + pdf + "\" does not exist");
    }
    
    auto page_str = std::to_string(page);

    const char *args[] = {
        PDFTOPNG_BIN,
        "-f", page_str.c_str(),  "-l", page_str.c_str(),
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