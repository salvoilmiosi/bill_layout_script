#include "pdf_to_image.h"

#include <filesystem>
#include <iostream>

#include "../shared/pipe.h"

wxImage pdf_to_image(const std::string &app_dir, const std::string &pdf, int page) {
    if (!wxFileExists(pdf)) {
        throw xpdf_error(std::string("File \"") + pdf + "\" does not exist");
    }
    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/xpdf/pdftopng", app_dir.c_str());
    
    char page_str[16];
    snprintf(page_str, 16, "%d", page);

    char temp_str[FILENAME_MAX];
    snprintf(temp_str, FILENAME_MAX, "%s/temp", app_dir.c_str());

    const char *args[] = {
        cmd_str,
        "-f", page_str,  "-l", page_str,
        pdf.c_str(), temp_str,
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::cout << output << std::endl;

    char base_filename[32];
    snprintf(base_filename, 32, "temp-%06d.png", page);

    std::string filename = app_dir + '/' + base_filename;

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