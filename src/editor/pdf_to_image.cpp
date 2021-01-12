#include "pdf_to_image.h"

#include <wx/filename.h>

#include <poppler/cpp/poppler-page-renderer.h>

#include <cstring>

wxImage pdf_to_image(const pdf_document &doc, int page) {
    auto &p = doc.get_page(page);
    poppler::page_renderer renderer;

    renderer.set_image_format(poppler::image::format_enum::format_rgb24);
    renderer.set_render_hint(poppler::page_renderer::antialiasing);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing);

    static constexpr double resolution = 144.0;

    poppler::image output = renderer.render_page(&p, resolution, resolution);

    size_t data_len = output.bytes_per_row() * output.height();
    unsigned char *data = (unsigned char *)malloc(data_len);
    std::memcpy(data, output.const_data(), data_len);

    return wxImage(output.width() + 1, output.height(), data);
}