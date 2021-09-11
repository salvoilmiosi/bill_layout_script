#include "pdf_document.h"

#include <fstream>
#include <regex>

#include <poppler-page-renderer.h>

using namespace bls;

void pdf_rect::rotate(int amt) {
    switch (amt % 4) {
    case 0:
        break;
    case 1:
    case -3:
        y = std::exchange(x, 1.0 - y - h);
        std::swap(w, h);
        break;
    case 2:
    case -2:
        x = 1.0 - x - w;
        y = 1.0 - y - h;
        break;
    case 3:
    case -1:
        x = std::exchange(y, 1.0 - x - w);
        std::swap(w, h);
        break;
    }
}

void pdf_document::open(const std::filesystem::path &filename) {
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (ifs.fail()) {
        throw file_error(intl::translate("CANT_OPEN_FILE", filename.string()));
    }
    std::vector<char> file_data{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    m_document.reset(poppler::document::load_from_data(&file_data));
    if (!m_document) {
        throw file_error(intl::translate("INVALID_PDF_DOCUMENT"));
    }

    m_filename = filename;

    m_pages.clear();
    for (int i=0; i < m_document->pages(); ++i) {
        m_pages.emplace_back(m_document->create_page(i));
    }
}

std::string poppler_string_to_std(const poppler::ustring &ustr) {
    auto arr = ustr.to_utf8();
    std::erase_if(arr, [](auto ch) {
        return ch == '\f'
#ifdef _WIN32
        || ch == '\r'
#endif
        ;
    });
    return {arr.begin(), arr.end()};
}

std::string pdf_document::get_text(const pdf_rect &rect) const {
    if (!isopen() || rect.page > num_pages() || rect.page <= 0) return "";
    const auto &page = *m_pages[rect.page - 1];
    auto pgrect = page.page_rect();
    poppler::rectf poppler_rect(rect.x * pgrect.width(), rect.y * pgrect.height(), rect.w * pgrect.width(), rect.h * pgrect.height());
    return poppler_string_to_std(page.text(poppler_rect, enums::get_data(rect.mode)));
}

std::string pdf_document::get_page_text(const pdf_rect &rect) const {
    if (!isopen() || rect.page > num_pages() || rect.page <= 0) return "";
    const auto &page = *m_pages[rect.page - 1];
    return poppler_string_to_std(page.text(poppler::rectf(), enums::get_data(rect.mode)));
}

pdf_image pdf_document::render_page(int page, int rotation) const {
    poppler::page_renderer renderer;
    renderer.set_image_format(poppler::image::format_enum::format_rgb24);
    renderer.set_render_hint(poppler::page_renderer::antialiasing);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing);

    constexpr double resolution = 150.0;

    poppler::image output = renderer.render_page(m_pages[page-1].get(), resolution, resolution, -1, -1, -1, -1, static_cast<poppler::rotation_enum>(rotation));
    const char *src_ptr = output.const_data();

    pdf_image ret(output.width(), output.height());
    
    unsigned char *dst_ptr = ret.data();
    for (size_t i=0; i<output.height(); ++i) {
        std::memcpy(dst_ptr, src_ptr, output.width() * 3);
        src_ptr += output.bytes_per_row();
        dst_ptr += output.width() * 3;
    }

    return ret;
}