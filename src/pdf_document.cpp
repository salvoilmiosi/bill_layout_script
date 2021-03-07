#include "pdf_document.h"

#include <fstream>
#include <regex>

#include "utils.h"

void pdf_document::open(const std::filesystem::path &filename) {
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
    if (rect.page > num_pages() || rect.page <= 0) return "";

    auto poppler_mode = poppler_modes[int(rect.mode)];

    auto to_stdstring = [](const poppler::ustring &ustr) {
        auto arr = ustr.to_utf8();
        std::string str(arr.begin(), arr.end());
        string_trim(str);
        return str;
    };

    switch (rect.type) {
    case BOX_RECTANGLE: {
        auto &page = get_page(rect.page);
        auto pgrect = page.page_rect();
        poppler::rectf poppler_rect(rect.x * pgrect.width(), rect.y * pgrect.height(), rect.w * pgrect.width(), rect.h * pgrect.height());
        return to_stdstring(page.text(poppler_rect, poppler_mode));
    }
    case BOX_PAGE:
        return to_stdstring(get_page(rect.page).text(poppler::rectf(), poppler_mode));
    case BOX_WHOLEFILE: {
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