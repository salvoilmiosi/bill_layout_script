#include "image_panel.h"

#include "../shared/xpdf.h"

#include <wx/dcclient.h>
#include <wx/dcbuffer.h>

BEGIN_EVENT_TABLE(wxImagePanel, wxScrolledWindow)
    EVT_PAINT(wxImagePanel::OnPaint)
END_EVENT_TABLE()

constexpr int SCROLL_RATE_X = 20;
constexpr int SCROLL_RATE_Y = 20;

wxImagePanel::wxImagePanel(wxWindow *parent) : wxScrolledWindow(parent) {
    SetScrollRate(SCROLL_RATE_X, SCROLL_RATE_Y);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

wxImagePanel::~wxImagePanel() {
    if (image)
        delete image;
    image = nullptr;
}

void wxImagePanel::setImage(const wxImage &new_image) {
    if (image)
        delete image;
    image = new wxImage(new_image);
    rescale(scale, wxIMAGE_QUALITY_HIGH);
}

void wxImagePanel::rescale(float factor, wxImageResizeQuality quality) {
    scale = factor;
    if (image) {
        scaled_width = image->GetWidth() * scale;
        scaled_height = image->GetHeight() * scale;
        SetVirtualSize(scaled_width, scaled_height);
        scaled_image = image->Scale(image->GetWidth() * scale, image->GetHeight() * scale, quality);
        Refresh();
    }
}

bool wxImagePanel::render(wxDC &dc) {
    dc.Clear();
    if (image) {
        scrollx = GetScrollPos(wxHORIZONTAL) * SCROLL_RATE_X;
        scrolly = GetScrollPos(wxVERTICAL) * SCROLL_RATE_Y;

        wxBitmap bitmap(scaled_image);
        
        dc.DrawBitmap(bitmap, -scrollx, -scrolly, false);
        return true;
    }
    return false;
}

void wxImagePanel::OnPaint(wxPaintEvent &evt) {
    wxBufferedPaintDC dc(this);
    render(dc);
    evt.Skip();
}