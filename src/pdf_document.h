#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>
#include <filesystem>

#include <poppler-document.h>
#include <poppler-page.h>

#include "utils.h"

#define BOX_TYPES { \
    BOX(RECTANGLE,  "Rettangolo"), \
    BOX(PAGE,       "Pagina"), \
    BOX(WHOLEFILE,  "File"), \
    BOX(NOREAD,     "Nessuna Lettura") \
}

#define BOX(x, y) BOX_##x
enum box_type : uint8_t BOX_TYPES;
#undef BOX
#define BOX(x, y) #x
constexpr const char *box_type_strings[] = BOX_TYPES;
#undef BOX
#define BOX(x, y) y
constexpr const char *box_type_labels[] = BOX_TYPES;
#undef BOX

#define READ_MODES { \
    MODE(DEFAULT,  "Default",  non_raw_non_physical_layout), \
    MODE(LAYOUT,   "Layout",   physical_layout), \
    MODE(RAW,      "Grezza",   raw_order_layout), \
}

#define MODE(x, y, z) MODE_##x
enum read_mode : uint8_t READ_MODES;
#undef MODE
#define MODE(x, y, z) #x
static const char *read_mode_strings[] = READ_MODES;
#undef MODE
#define MODE(x, y, z) y
static const char *read_mode_labels[] = READ_MODES;
#undef MODE
#define MODE(x, y, z) poppler::page::text_layout_enum::z
static poppler::page::text_layout_enum poppler_modes[] = READ_MODES;
#undef MODE

#define PDF_FLAGS { \
    F(NOTRIM, "Mantieni Spazi"), \
}

#define F(x, y) POS_PDF_##x
enum PDF_FLAGS;
#undef F
#define F(x, y) PDF_##x = (1 << POS_PDF_##x)
enum pdf_flags : flags_t PDF_FLAGS;
#undef F
#define F(x, y) #x
static const char *pdf_flags_names[] = PDF_FLAGS;
#undef F
#define F(x, y) y
static const char *pdf_flags_labels[] = PDF_FLAGS;
#undef F

struct pdf_rect {
    int page = 0;
    double x, y;
    double w, h;
    read_mode mode = MODE_DEFAULT;
    box_type type = BOX_RECTANGLE;
    flags_t flags = 0;
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

    std::string get_text(pdf_rect rect, int rotation = 0) const;

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
    pdf_error(auto &&message) : std::runtime_error(std::forward<decltype(message)>(message)) {}
};

#endif // __PDF_DOCUMENT_H__