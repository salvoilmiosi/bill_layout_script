#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>

#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

enum class box_type : uint8_t              { BOX_RECTANGLE, BOX_PAGE, BOX_FILE, BOX_NO_READ };
constexpr const char *box_type_strings[] = { "RECTANGLE",   "PAGE",   "FILE",   "NOREAD" };
constexpr const char *box_type_labels[] =  { "Rettangolo",  "Pagina", "File",   "Nessuna Lettura" };

enum class read_mode : uint8_t             { MODE_DEFAULT, MODE_LAYOUT, MODE_RAW };
constexpr const char *read_mode_strings[] = { "DEFAULT",    "LAYOUT",    "RAW" };
constexpr const char *read_mode_labels[] =  { "Default",    "Layout",    "Grezza" };

struct pdf_rect {
    float x, y;
    float w, h;
    uint8_t page = 0;
    read_mode mode = read_mode::MODE_DEFAULT;
    box_type type = box_type::BOX_RECTANGLE;
};

class pdf_document {
public:
    pdf_document() = default;

    explicit pdf_document(const std::string &filename) {
        open(filename);
    }

    void open(const std::string &filename);
    bool isopen() const {
        return m_document != nullptr;
    }

    std::string get_text(const pdf_rect &rect) const;

    const std::string &filename() const { return m_filename; }
    int num_pages() const { return m_num_pages; }

    const poppler::page &get_page(int page) const {
        return *m_pages[page - 1];
    }
    
private:
    std::string m_filename;
    int m_num_pages = 0;

    std::unique_ptr<poppler::document> m_document;
    std::vector<std::unique_ptr<poppler::page>> m_pages;
};

struct pdf_error {
    const std::string message;
};

#endif // __PDF_DOCUMENT_H__