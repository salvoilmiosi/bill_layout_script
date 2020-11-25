#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <string>

enum box_type                              { BOX_RECTANGLE, BOX_PAGE, BOX_FILE, BOX_DISABLED };
constexpr const char *box_type_strings[] = { "RECTANGLE",   "PAGE",   "FILE",   "DISABLED" };
constexpr const char *box_type_labels[] =  { "Rettangolo",  "Pagina", "File",   "Disabilitato" };

enum read_mode                              { MODE_DEFAULT, MODE_LAYOUT, MODE_RAW };
constexpr const char *read_mode_strings[] = { "DEFAULT",    "LAYOUT",    "RAW" };
constexpr const char *read_mode_labels[] =  { "Default",    "Layout",    "Grezza" };
constexpr const char *read_mode_options[] = { nullptr,      "-layout",   "-raw" };

struct pdf_rect {
    float x, y;
    float w, h;
    int page = 0;
    read_mode mode = MODE_DEFAULT;
    box_type type = BOX_RECTANGLE;
};

class pdf_document {
public:
    pdf_document() {}
    explicit pdf_document(const std::string &filename) {
        open(filename);
    }

    void open(const std::string &filename);
    bool isopen() const {
        return !m_filename.empty();
    }

    std::string get_text(const pdf_rect &rect) const;

    const std::string &filename() const { return m_filename; }
    int num_pages() const { return m_num_pages; }
    float width() const { return m_width; }
    float height() const { return m_height; }
    
private:
    std::string m_filename;
    int m_num_pages = 0;
    float m_width = 0;
    float m_height = 0;
};

struct pdf_error {
    const std::string message;

    pdf_error(const std::string &message) : message(message) {}
};

#endif // __PDF_DOCUMENT_H__