#include "pdf_to_image.h"

namespace xpdf {

wxImage pdf_to_image(const std::string &app_dir, const std::string &pdf, int page) {
    std::string args;
    args += "-f " + std::to_string(page) + " ";
    args += "-l " + std::to_string(page) + " ";
    args += "\"" + pdf + "\" \"" + app_dir + "/temp\"";

    std::string output = open_process(app_dir + "/xpdf/pdftopng.exe", args)->read_all();

    char base_filename[32];
    snprintf(base_filename, 32, "temp-%06d.png", page);

    std::string filename = app_dir + '/' + base_filename;

    wxImage img;
    img.AddHandler(new wxPNGHandler);

    if (wxFileExists(filename)) {
        img.LoadFile(filename, wxBITMAP_TYPE_PNG);
        wxRemoveFile(filename);
        
        if (img.IsOk()) {
            return img;
        }
    }
    throw pipe_error("Could not open image");
}

}