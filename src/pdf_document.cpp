#include "pdf_document.h"

#include <fstream>
#include <regex>

#include <ErrorCodes.h>
#include <GlobalParams.h>
#include <TextOutputDev.h>
#include <SplashOutputDev.h>
#include <Splash/SplashBitmap.h>

using namespace bls;

static const GlobalParamsIniter errorFunction([](ErrorCategory, Goffset pos, const char *msg) {
    if (pos >= 0) {
        std::cerr << "poppler error (" << pos << "): ";
    } else {
        std::cerr << "poppler error: ";
    }
    std::cerr << msg;
});

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
    m_document = std::make_unique<PDFDoc>(new GooString(filename.string()));
    if (!m_document->isOk() && m_document->getErrorCode() != errEncrypted) {
        m_document.reset();
        throw file_error(intl::translate("CANT_OPEN_FILE", filename.string()));
    }
}

template<bool Slice> static std::string do_get_text(PDFDoc *doc, const pdf_rect &rect) {
    std::string ret;
    if (!doc || rect.page > doc->getNumPages() || rect.page < 1) return ret;

    TextOutputDev td([](void *stream, const char *text, int len) {
        static_cast<std::string *>(stream)->append(text, len);
    }, &ret, rect.mode == read_mode::LAYOUT, 0, rect.mode == read_mode::RAW, false);
    
    td.setTextEOL(eolUnix);
    td.setTextPageBreaks(false);

    if constexpr (Slice) {
        const double w = doc->getPageCropWidth(rect.page);
        const double h = doc->getPageCropHeight(rect.page);

        doc->displayPageSlice(&td, rect.page, 72, 72, 0, false, true, false,
            rect.x * w, rect.y * h, rect.w * w, rect.h * h);
    } else {
        doc->displayPage(&td, rect.page, 72, 72, 0, false, true, false);
    }
    return ret;
}

std::string pdf_document::get_text(const pdf_rect &rect) const {
    return do_get_text<true>(m_document.get(), rect);
}

std::string pdf_document::get_page_text(const pdf_rect &rect) const {
    return do_get_text<false>(m_document.get(), rect);
}

pdf_image pdf_document::render_page(int page, int rotation) const {
    SplashColor white{0xff, 0xff, 0xff};
    SplashOutputDev dev(splashModeRGB8, 4, false, white, true);
    dev.setFontAntialias(true);
    dev.setVectorAntialias(true);
    dev.setFreeTypeHinting(false, false);
    dev.startDoc(m_document.get());

    constexpr double resolution = 150.0;

    m_document->displayPage(&dev, page, resolution, resolution, rotation * 90, false, true, false);

    SplashBitmap *bitmap = dev.getBitmap();
    pdf_image ret(bitmap->getWidth(), bitmap->getHeight());

    const int bpr = (ret.width() * 3 + 3) & (~0b11);
    
    unsigned char *src_ptr = bitmap->getDataPtr();
    unsigned char *dst_ptr = ret.data();
    for (size_t i=0; i<ret.height(); ++i) {
        std::memcpy(dst_ptr, src_ptr, ret.width() * 3);
        src_ptr += bpr;
        dst_ptr += ret.width() * 3;
    }
    return ret;
}