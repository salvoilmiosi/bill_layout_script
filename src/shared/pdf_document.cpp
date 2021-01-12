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

    if (m_num_pages == 0) {
        throw pdf_error(fmt::format("Errore: Il file {} contiene 0 pagine", filename));
    }

    auto rect = m_pages.front()->page_rect();
    m_width = rect.width();
    m_height = rect.height();
}

std::string pdf_document::get_text(const pdf_rect &rect) const {
    if (!isopen()) return "";
    if (rect.page > m_num_pages) return "";

    poppler::page::text_layout_enum poppler_mode;
    switch (rect.mode) {
    case read_mode::MODE_DEFAULT:
        poppler_mode = poppler::page::text_layout_enum::non_raw_non_physical_layout;
        break;
    case read_mode::MODE_LAYOUT: 
        poppler_mode = poppler::page::text_layout_enum::physical_layout;
        break;
    case read_mode::MODE_RAW:
        poppler_mode = poppler::page::text_layout_enum::raw_order_layout;
        break;
    }

    std::string ret;

    auto to_string = [](const poppler::ustring &str) {
        auto arr = str.to_utf8();
        return std::string(arr.begin(), arr.end());
    };

    switch (rect.type) {
    case box_type::BOX_RECTANGLE:
    {
        poppler::rectf poppler_rect(rect.x * m_width, rect.y * m_height, rect.w * m_width, rect.h * m_height);
        ret = to_string(get_page(rect.page).text(poppler_rect, poppler_mode));
        break;
    }
    case box_type::BOX_PAGE:
        ret = to_string(get_page(rect.page).text(poppler::rectf(), poppler_mode));
        break;
    case box_type::BOX_FILE:
    {
        for (auto &page : m_pages) {
            ret.append(to_string(page->text(poppler::rectf(), poppler_mode)));
            ret += '\n';
        }
        break;
    }
    default:
        break;
    }

    string_trim(ret);
    return ret;
}