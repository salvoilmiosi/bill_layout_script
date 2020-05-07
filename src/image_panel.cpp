#include "image_panel.h"

#include "xpdf.h"

BEGIN_EVENT_TABLE(wxImagePanel, wxScrolledWindow)
    EVT_PAINT(wxImagePanel::OnPaint)
    EVT_SCROLLWIN(wxImagePanel::OnScroll)
END_EVENT_TABLE()

wxImagePanel::wxImagePanel(wxWindow *parent) : wxScrolledWindow(parent) {
    SetScrollRate(20, 20);
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

void wxImagePanel::paintNow() {
    wxClientDC dc(this);
    render(dc);
}

void wxImagePanel::rescale(float factor) {
    if (image) {
        scale = factor;
        SetVirtualSize(image->GetWidth() * scale, image->GetHeight() * scale);
        scaled_image = image->Scale(image->GetWidth() * scale, image->GetHeight() * scale);
        paintNow();
    }
}

void wxImagePanel::render(wxDC &dc) {
    if (image) {
        wxBitmap bitmap(scaled_image);
        dc.Clear();
        dc.DrawBitmap(bitmap,
            -GetScrollPos(wxHORIZONTAL) * 20,
            -GetScrollPos(wxVERTICAL) * 20, false);
    }
}

void wxImagePanel::OnPaint(wxPaintEvent &evt) {
    wxPaintDC dc(this);
    render(dc);
}

void wxImagePanel::OnScroll(wxScrollWinEvent &evt) {
    evt.Skip();
}
