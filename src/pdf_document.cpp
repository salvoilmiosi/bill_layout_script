#include "pdf_document.h"

#include <fstream>
#include <regex>

#include "utils.h"

void pdf_document::open(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::in | std::ios::binary);
    if (ifs.fail()) {
        throw pdf_error("Impossibile aprire il file");
    }
    std::vector<char> file_data{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    m_document.reset(poppler::document::load_from_data(&file_data));
    if (!m_document) {
        throw pdf_error("Documento pdf invalido");
    }

    m_filename = filename;

    m_pages.clear();
    for (size_t i=0; i < m_document->pages(); ++i) {
        m_pages.emplace_back(m_document->create_page(i));
    }
}

std::string pdf_document::get_text(const pdf_rect &rect) const {
    if (!isopen()) return "";
    if (rect.page > num_pages()) return "";

    auto poppler_mode = [](read_mode mode) {
        switch (mode) {
        case read_mode::MODE_LAYOUT:  return poppler::page::text_layout_enum::physical_layout;
        case read_mode::MODE_RAW:     return poppler::page::text_layout_enum::raw_order_layout;
        case read_mode::MODE_DEFAULT:
        default:                      return poppler::page::text_layout_enum::non_raw_non_physical_layout;
        }
    }(rect.mode);

    auto to_stdstring = [](const poppler::ustring &ustr) {
        auto arr = ustr.to_utf8();
        std::string str(arr.begin(), arr.end());
        string_trim(str);
        return str;
    };

    switch (rect.type) {
    case box_type::BOX_RECTANGLE:
    {
        auto &page = get_page(rect.page);
        auto pgrect = page.page_rect();
        poppler::rectf poppler_rect(rect.x * pgrect.width(), rect.y * pgrect.height(), rect.w * pgrect.width(), rect.h * pgrect.height());
        return to_stdstring(page.text(poppler_rect, poppler_mode));
    }
    case box_type::BOX_PAGE:
        return to_stdstring(get_page(rect.page).text(poppler::rectf(), poppler_mode));
    case box_type::BOX_FILE:
    {
        poppler::ustring ret;
        for (auto &page : m_pages) {
            ret.append(page->text(poppler::rectf(), poppler_mode));
        }
        return to_stdstring(ret);
    }
    default:
        return "";
    }

}