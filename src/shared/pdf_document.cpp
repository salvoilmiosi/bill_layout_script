#include "pdf_document.h"

#include <filesystem>
#include <regex>

#include <fmt/format.h>

#include <poppler/cpp/poppler-page.h>

#include "subprocess.h"
#include "utils.h"

void pdf_document::open(const std::string &filename) {
    m_document.reset(poppler::document::load_from_file(filename));
    if (!m_document) {
        throw pdf_error(fmt::format("Impossibile aprire il file {}", filename));
    }

    m_filename = filename;

    m_pages.clear();
    m_num_pages = m_document->pages();

    for (size_t i=0; i<m_num_pages; ++i) {
        m_pages.emplace_back(m_document->create_page(i));
    }
}

std::string pdf_document::get_text(const pdf_rect &rect) const {
    if (!isopen()) return "";
    if (rect.page > m_num_pages) return "";

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