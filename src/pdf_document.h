#ifndef __PDF_DOCUMENT_H__
#define __PDF_DOCUMENT_H__

#include <vector>
#include <string>
#include <memory>
#include <tuple>
#include <filesystem>

#include <poppler-document.h>
#include <poppler-page.h>

#include "utils/utils.h"

namespace bls {
    DEFINE_ENUM_DATA_IN_NS(bls, read_mode,
        (DEFAULT,  poppler::page::non_raw_non_physical_layout)
        (LAYOUT,   poppler::page::physical_layout)
        (RAW,      poppler::page::raw_order_layout)
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
        size_t m_capacity = 0;

        void make_data() {
            m_capacity = m_width * m_height * 3;
            m_data = (unsigned char *) std::malloc(m_capacity);
        }

    public:
        pdf_image() = default;
        ~pdf_image() {
            delete m_data;
        }
        pdf_image(int width, int height) : m_width(width), m_height(height) {
            make_data();
        }
        pdf_image(const pdf_image &other) : pdf_image(other.m_width, other.m_height) {
            std::memcpy(m_data, other.m_data, m_capacity);
        }
        pdf_image(pdf_image &&other) noexcept : m_width(other.m_width), m_height(other.m_height) {
            m_data = other.m_data;
            other.m_data = nullptr;
        }
        pdf_image &operator = (const pdf_image &other) {
            m_width = other.m_width;
            m_height = other.m_height;
            if (m_capacity < other.m_capacity) {
                delete m_data;
                make_data();
            }
            std::memcpy(m_data, other.m_data, other.m_capacity);
            return *this;
        }
        pdf_image &operator = (pdf_image &&other) noexcept {
            m_width = other.m_width;
            m_height = other.m_height;
            m_capacity = other.m_capacity;
            std::swap(m_data, other.m_data);
            return *this;
        }

        int width() const noexcept { return m_width; }
        int height() const noexcept { return m_height; }

        unsigned char *data() const noexcept { return m_data; }
        void release() { m_data = nullptr; }
    };

    class pdf_document {
    public:
        pdf_document() = default;

        explicit pdf_document(const std::filesystem::path &filename) {
            open(filename);
        }

        void open(const std::filesystem::path &filename);

        bool isopen() const { return m_document != nullptr; }

        const std::filesystem::path &filename() const { return m_filename; }
        
        int num_pages() const { return m_pages.size(); }

        std::string get_text(const pdf_rect &rect) const;
        
        std::string get_page_text(const pdf_rect &rect) const;

        pdf_image render_page(int page, int rotation = 0) const;
        
    private:
        std::filesystem::path m_filename;

        std::unique_ptr<poppler::document> m_document;
        std::vector<std::unique_ptr<poppler::page>> m_pages;
    };

}

#endif // __PDF_DOCUMENT_H__