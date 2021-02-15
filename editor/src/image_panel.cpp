#include "image_panel.h"

#include "pdf_document.h"

#include <wx/dcbuffer.h>

constexpr int SCROLL_RATE = 20;

wxImagePanel::wxImagePanel(wxWindow *parent) : wxScrolledCanvas(parent) {
    SetScrollRate(SCROLL_RATE, SCROLL_RATE);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void wxImagePanel::setImage(const wxImage &new_image) {
    raw_image = new_image;
    rescale(m_scale, wxIMAGE_QUALITY_HIGH);
}

void wxImagePanel::rescale(float factor, wxImageResizeQuality quality) {
    m_scale = factor;
    if (raw_image.IsOk()) {
        scaled_image = raw_image.Scale(
            raw_image.GetWidth() * m_scale,
            raw_image.GetHeight() * m_scale, quality);
        SetVirtualSize(scaled_image.GetSize());
        Refresh();
    }
}

void wxImagePanel::render(wxDC &dc) {
    dc.DrawBitmap(scaled_image, 0, 0);
}

void wxImagePanel::OnDraw(wxDC &dc) {
    if (scaled_image.IsOk()) {
        wxBufferedDC buf_dc(&dc, wxSize(
            std::max(scaled_image.GetWidth(), GetSize().GetWidth()),
            std::max(scaled_image.GetHeight(), GetSize().GetHeight())
        ), wxBUFFER_VIRTUAL_AREA);
        buf_dc.Clear();
        render(buf_dc);
    }
}