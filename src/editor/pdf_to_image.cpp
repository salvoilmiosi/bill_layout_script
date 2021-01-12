#include "pdf_to_image.h"

#include <wx/filename.h>

#include <poppler/cpp/poppler-page-renderer.h>

#include <cstring>

struct default_renderer : poppler::page_renderer {
    default_renderer() {
        set_image_format(poppler::image::format_enum::format_rgb24);
        set_render_hint(poppler::page_renderer::antialiasing);
        set_render_hint(poppler::page_renderer::text_antialiasing);
    }
};

wxImage pdf_to_image(const pdf_document &doc, int page) {
    static default_renderer renderer;

    static constexpr double resolution = 150.0;

    poppler::image output = renderer.render_page(&doc.get_page(page), resolution, resolution);
    const char *src_ptr = output.const_data();

    unsigned char *data = (unsigned char *) std::malloc(output.width() * output.height() * 3);
    unsigned char *dst_ptr = data;
    for (size_t i=0; i<output.height(); ++i) {
        std::memcpy(dst_ptr, src_ptr, output.width() * 3);
        src_ptr += output.bytes_per_row();
        dst_ptr += output.width() * 3;
    }

    return wxImage(output.width(), output.height(), data);
}