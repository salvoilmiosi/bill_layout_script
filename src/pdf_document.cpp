#include "pdf_document.h"

#include <fstream>
#include <regex>

#include "utils.h"

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

    auto poppler_mode = EnumData<poppler::page::text_layout_enum>(rect.mode);

    auto to_stdstring = [flags = rect.flags](const poppler::ustring &ustr) -> std::string {
        auto arr = ustr.to_utf8();
        std::erase_if(arr, [](auto ch) {
            return ch == '\f'
#ifdef _WIN32
            || ch == '\r'
#endif
            ;
        });
        if (flags & box_flags::TRIM) {
            return string_trim({arr.begin(), arr.end()});
        } else {
            return {arr.begin(), arr.end()};
        }
    };

    switch (rect.type) {
    case box_type::RECTANGLE: {
        if (rect.page > num_pages() || rect.page <= 0) return "";
        auto &page = get_page(rect.page);
        auto pgrect = page.page_rect();
        
        poppler::rectf poppler_rect(rect.x * pgrect.width(), rect.y * pgrect.height(), rect.w * pgrect.width(), rect.h * pgrect.height());
        return to_stdstring(page.text(poppler_rect, poppler_mode));
    }
    case box_type::PAGE:
        if (rect.page > num_pages() || rect.page <= 0) return "";
        return to_stdstring(get_page(rect.page).text(poppler::rectf(), poppler_mode));
    case box_type::WHOLEFILE: {
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