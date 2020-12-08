#include "pdf_to_image.h"

#include <wx/filename.h>
#include <wx/mstream.h>

#include "subprocess.h"

class SubprocessStream : public wxInputStream {
public:
    SubprocessStream(subprocess &proc): proc(proc) {}

protected:
    virtual size_t OnSysRead(void *buffer, size_t bufsize) override {
        return proc.read_stdout(bufsize, buffer);
    }

private:
    subprocess &proc;
};

wxImage pdf_to_image(const pdf_document &doc, int page) {
    auto page_str = std::to_string(page);

#if defined(WIN32) || defined(_WIN32)
    // bisogna su windows creare un file temporaneo
    // perchè windows converte tutti i \n in \r\n nelle pipe, corrompendo i dati
    
    wxString temp_file = wxFileName::CreateTempFileName("pdf");
    wxRenameFile(temp_file, temp_file + ".png"); // pdftocairo aggiunge l'estensione al nome file

    const char *args[] = {
        "pdftocairo",
        "-f", page_str.c_str(), "-l", page_str.c_str(),
        "-png", "-singlefile",
        doc.filename().c_str(), temp_file.c_str(),
        nullptr
    };
    
    temp_file += ".png";

    open_process(args)->read_all();

    if (wxFileExists(temp_file)) {
        wxImage img(temp_file, wxBITMAP_TYPE_PNG);
        wxRemoveFile(temp_file);
        
        if (img.IsOk()) {
            return img;
        }
    }
#else
    const char *args[] = {
        "pdftocairo", "-q",
        "-f", page_str.c_str(), "-l", page_str.c_str(),
        "-png", "-singlefile",
        doc.filename().c_str(), "-",
        nullptr
    };

    auto process = open_process(args);
    SubprocessStream stream(*process);
    wxImage img(stream, wxBITMAP_TYPE_PNG);
    
    if (img.IsOk()) {
        return img;
    }

#endif
    throw pdf_error("Could not open image");
}