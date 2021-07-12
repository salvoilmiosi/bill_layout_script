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
#include "enums.h"

namespace bls {
    typedef std::tuple<const char *, poppler::page::text_layout_enum> read_mode_tuple;

    DEFINE_ENUM_DATA_IN_NS(bls, read_mode, read_mode_tuple,
        (DEFAULT,  "Default", poppler::page::non_raw_non_physical_layout)
        (LAYOUT,   "Layout",  poppler::page::physical_layout)
        (RAW,      "Grezza",  poppler::page::raw_order_layout)
    )

    DEFINE_ENUM_FLAGS_DATA_IN_NS(bls, box_flags, const char *,
        (DISABLED,  "Disabilita")
        (PAGE,      "Leggi Pagina")
        (NOREAD,    "Nessuna Lettura")
        (SPACER,    "Spaziatore")
        (TRIM,      "Taglia Spazi")
    )
    
    struct pdf_rect {
        double x, y, w, h;
        int page;
        read_mode mode = read_mode::DEFAULT;
        enums::bitset<box_flags> flags;

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

}

#endif // __PDF_DOCUMENT_H__