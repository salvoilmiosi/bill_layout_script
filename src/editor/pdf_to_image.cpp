#include "pdf_to_image.h"

#include <wx/filename.h>

#include "subprocess.h"

class myStdInputStreamAdapter : public wxInputStream {
public:
    myStdInputStreamAdapter(std::istream &s): stream{s} {}

protected:
    std::istream &stream;

    virtual size_t OnSysRead(void *buffer, size_t bufsize) {
        std::streamsize size = 0;

        stream.peek();

        if (stream.fail() || stream.bad()) {
            m_lasterror = wxSTREAM_READ_ERROR;
        } else if (stream.eof()) {
            m_lasterror = wxSTREAM_EOF;
        } else {
            size = stream.readsome(static_cast<std::istream::char_type *>(buffer), bufsize);
        }

        return size;
    }
};

wxImage pdf_to_image(const pdf_document &doc, int page) {
    auto page_str = std::to_string(page);

#if defined(_WIN32) && !defined(POPPLER_FIX)
    // bisogna su windows creare un file temporaneo
    // perchè windows converte tutti i \n in \r\n nelle pipe, corrompendo i dati
    
    wxString temp_file = wxFileName::CreateTempFileName("pdf");
    wxRenameFile(temp_file, temp_file + ".png"); // pdftocairo aggiunge l'estensione al nome file
    
    if (subprocess(arguments(
        "pdftocairo",
        "-f", page_str, "-l", page_str,
        "-png", "-singlefile",
        doc.filename(), temp_file
    )).wait_finished() != 0) {
        throw pdf_error("Could not open image");
    }

    temp_file += ".png";

    if (wxFileExists(temp_file)) {
        wxImage img(temp_file, wxBITMAP_TYPE_PNG);
        wxRemoveFile(temp_file);
        
        if (img.IsOk()) {
            return img;
        }
    }
#else
    subprocess process(arguments(
        "pdftocairo", "-q",
        "-f", page_str, "-l", page_str,
        "-png", "-singlefile",
        doc.filename(), "-"
    ));
    myStdInputStreamAdapter stream(process.stream_out);
    wxImage img(stream, wxBITMAP_TYPE_PNG);
    
    if (img.IsOk()) {
        return img;
    }

#endif
    throw pdf_error("Could not open image");
}