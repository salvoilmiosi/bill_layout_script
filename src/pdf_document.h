#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <filesystem>

#include <PDFDoc.h>

#include "utils/utils.h"

namespace bls {
    DEFINE_ENUM_IN_NS(bls, read_mode,
        (DEFAULT)
        (LAYOUT)
        (RAW)
    )
    
    struct pdf_rect {
        double x, y, w, h;
        int page;
        read_mode mode = read_mode::DEFAULT;

        void rotate(int amt);
    };

    class pdf_image {
    private:
        int m_width = 0;
        int m_height = 0;
        unsigned char *m_data = nullptr;

        void make_data();

    public:
        pdf_image(int width, int height) : m_width(width), m_height(height) {
            m_data = (unsigned char *) std::malloc(capacity());
        }

        pdf_image(int width, int height, unsigned char *data)
            : m_width(width), m_height(height), m_data(data) {}

        pdf_image() = default;
        ~pdf_image() {
            std::free(m_data);
        }

        pdf_image(const pdf_image &other) : pdf_image(other.m_width, other.m_height) {
            std::memcpy(m_data, other.m_data, capacity());
        }

        pdf_image(pdf_image &&other) noexcept : m_width(other.m_width), m_height(other.m_height) {
            m_data = other.m_data;
            other.m_data = nullptr;
        }

        pdf_image &operator = (const pdf_image &other);
        pdf_image &operator = (pdf_image &&other) noexcept;

        int width() const noexcept { return m_width; }
        int height() const noexcept { return m_height; }
        int capacity() const noexcept { return m_width * m_height * 3; }

        unsigned char *data() const noexcept { return m_data; }
        unsigned char *release() { return std::exchange(m_data, nullptr); }
    };

    class pdf_document {
    public:
        pdf_document() = default;

        explicit pdf_document(const std::filesystem::path &filename) {
            open(filename);
        }

        void open(const std::filesystem::path &filename);

        bool isopen() const { return m_document != nullptr; }

        std::filesystem::path filename() const {
            return m_document->getFileName()->toStr();
        }
        
        int num_pages() const {
            return m_document->getNumPages();
        }

        std::string get_text(const pdf_rect &rect) const;
        
        std::string get_page_text(const pdf_rect &rect) const;

        pdf_image render_page(int page, int rotation = 0) const;
        
    private:
        std::unique_ptr<PDFDoc> m_document;
    };

}

#endif // __PDF_DOCUMENT_H__