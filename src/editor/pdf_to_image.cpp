#include "pdf_to_image.h"

#include <iostream>
#include <wx/filename.h>

#include "subprocess.h"

wxImage pdf_to_image(const pdf_document &doc, int page) {
    auto page_str = std::to_string(page);

    wxString temp_file = wxFileName::CreateTempFileName("pdf");
    wxRemoveFile(temp_file);

    const char *args[] = {
        "pdftocairo",
        "-f", page_str.c_str(), "-l", page_str.c_str(),
        "-png", "-singlefile",
        doc.filename().c_str(), temp_file.c_str(),
        nullptr
    };

    std::string output = open_process(args)->read_all();
    std::cout << output << std::endl;

    wxImage img;

    if (wxFileExists(temp_file + ".png")) {
        img.LoadFile(temp_file + ".png", wxBITMAP_TYPE_PNG);
        wxRemoveFile(temp_file + ".png");
        
        if (img.IsOk()) {
            return img;
        }
    }
    throw pdf_error("Could not open image");
}