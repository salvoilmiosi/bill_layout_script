#include "pdf_to_image.h"

#include <wx/filename.h>

#include <poppler/cpp/poppler-page-renderer.h>

#include <cstring>

wxImage pdf_to_image(const pdf_document &doc, int page) {
    static poppler::page_renderer renderer;

    renderer.set_image_format(poppler::image::format_enum::format_rgb24);
    renderer.set_render_hint(poppler::page_renderer::antialiasing);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing);

    static constexpr double resolution = 150.0;

    poppler::image output = renderer.render_page(&doc.get_page(page), resolution, resolution);

    size_t data_len = output.bytes_per_row() * output.height();
    unsigned char *data = (unsigned char *)malloc(data_len);
    std::memcpy(data, output.const_data(), data_len);

    return wxImage(output.bytes_per_row() / 3, output.height(), data);
}