#include "image_panel.h"

#include "../shared/xpdf.h"

#include <wx/dcclient.h>

BEGIN_EVENT_TABLE(wxImagePanel, wxScrolledWindow)
    EVT_PAINT(wxImagePanel::OnPaint)
END_EVENT_TABLE()

constexpr int SCROLL_RATE_X = 20;
constexpr int SCROLL_RATE_Y = 20;

wxImagePanel::wxImagePanel(wxWindow *parent) : wxScrolledWindow(parent) {
    SetScrollRate(SCROLL_RATE_X, SCROLL_RATE_Y);
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
    rescale(scale);
}

void wxImagePanel::paintNow(bool clear) {
    wxClientDC dc(this);
    render(dc, clear);
}

void wxImagePanel::rescale(float factor) {
    scale = factor;
    if (image) {
        scaled_width = image->GetWidth() * scale;
        scaled_height = image->GetHeight() * scale;
        SetVirtualSize(scaled_width, scaled_height);
        scaled_image = image->Scale(image->GetWidth() * scale, image->GetHeight() * scale);
        paintNow(true);
    }
}

bool wxImagePanel::render(wxDC &dc, bool clear) {
    if (image) {
        scrollx = GetScrollPos(wxHORIZONTAL) * SCROLL_RATE_X;
        scrolly = GetScrollPos(wxVERTICAL) * SCROLL_RATE_Y;

        wxBitmap bitmap(scaled_image);
        
        if (clear) dc.Clear();
        dc.DrawBitmap(bitmap, -scrollx, -scrolly, false);
        return true;
    }
    return false;
}

void wxImagePanel::OnPaint(wxPaintEvent &evt) {
    wxPaintDC dc(this);
    render(dc);
    evt.Skip();
}