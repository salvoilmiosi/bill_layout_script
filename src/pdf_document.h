#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>
#include <filesystem>

#include <poppler-document.h>
#include <poppler-page.h>

enum class box_type : uint8_t              { BOX_RECTANGLE, BOX_PAGE, BOX_FILE, BOX_NO_READ };
constexpr const char *box_type_strings[] = { "RECTANGLE",   "PAGE",   "FILE",   "NOREAD" };
constexpr const char *box_type_labels[] =  { "Rettangolo",  "Pagina", "File",   "Nessuna Lettura" };

enum class read_mode : uint8_t             { MODE_DEFAULT, MODE_LAYOUT, MODE_RAW };
constexpr const char *read_mode_strings[] = { "DEFAULT",    "LAYOUT",    "RAW" };
constexpr const char *read_mode_labels[] =  { "Default",    "Layout",    "Grezza" };

struct pdf_rect {
    int page = 0;
    float x, y;
    float w, h;
    read_mode mode = read_mode::MODE_DEFAULT;
    box_type type = box_type::BOX_RECTANGLE;
};

class pdf_document {
public:
    pdf_document() = default;

    explicit pdf_document(const std::filesystem::path &filename) {
        open(filename);
    }

    void open(const std::filesystem::path &filename);
    bool isopen() const {
        return m_document != nullptr;
    }

    std::string get_text(const pdf_rect &rect) const;

    const std::filesystem::path &filename() const { return m_filename; }
    int num_pages() const { return m_pages.size(); }

    const poppler::page &get_page(int page) const {
        return *m_pages[page - 1];
    }
    
private:
    std::filesystem::path m_filename;

    std::unique_ptr<poppler::document> m_document;
    std::vector<std::unique_ptr<poppler::page>> m_pages;
};

struct pdf_error : std::runtime_error {
    pdf_error(const std::string &message) : std::runtime_error(message) {}
};

#endif // __PDF_DOCUMENT_H__