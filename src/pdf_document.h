#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <filesystem>

#include <poppler-document.h>
#include <poppler-page.h>

#include "utils.h"
#include "bitset.h"

DEFINE_ENUM_WITH_DATA(box_type,
    (RECTANGLE,  "Rettangolo")
    (PAGE,       "Pagina")
    (WHOLEFILE,  "File")
    (NOREAD,     "Nessuna Lettura")
)

DEFINE_ENUM_WITH_DATA(read_mode,
    (DEFAULT,  std::make_tuple("Default", poppler::page::non_raw_non_physical_layout))
    (LAYOUT,   std::make_tuple("Layout",  poppler::page::physical_layout))
    (RAW,      std::make_tuple("Grezza",  poppler::page::raw_order_layout))
)

DEFINE_FLAGS_WITH_DATA(box_flags,
    (DISABLED,  "Disabilita")
    (TRIM,      "Taglia Spazi")
)

struct pdf_rect {
    int page = 0;
    double x, y;
    double w, h;
    read_mode mode = read_mode::DEFAULT;
    box_type type = box_type::RECTANGLE;
    bitset<box_flags> flags;

    void rotate(int amt);
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
    template<typename T>
    pdf_error(T &&message) : std::runtime_error(std::forward<T>(message)) {}
};

#endif // __PDF_DOCUMENT_H__