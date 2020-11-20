#include "pdf_to_image.h"

#include <filesystem>
#include <iostream>

#include "subprocess.h"

wxImage pdf_to_image(const pdf_document &doc, int page) {
    auto page_str = std::to_string(page);

    const char *args[] = {
        "pdftocairo",
        "-f", page_str.c_str(),  "-l", page_str.c_str(),
        "-png", "-singlefile",
        doc.filename().c_str(), "temp",
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
    throw pdf_error("Could not open image");
}