#include "image_panel.h"

#include "xpdf.h"

BEGIN_EVENT_TABLE(wxImagePanel, wxScrolledWindow)
    EVT_PAINT(wxImagePanel::OnPaint)
    EVT_SCROLLWIN(wxImagePanel::OnScroll)
END_EVENT_TABLE()

wxImagePanel::wxImagePanel(wxWindow *parent) : wxScrolledWindow(parent) { }

wxImagePanel::~wxImagePanel() {
    if (bitmap)
        delete bitmap;
    bitmap = nullptr;
}

void wxImagePanel::setImage(const wxImage &image) {
    if (bitmap != nullptr)
        delete bitmap;
    bitmap = new wxBitmap(image);
    SetVirtualSize(image.GetSize());
    SetScrollRate(20, 20);
    paintNow();
}

void wxImagePanel::paintNow() {
    wxClientDC dc(this);
    render(dc);
}

void wxImagePanel::render(wxDC &dc) {
    if (bitmap) {
        dc.DrawBitmap(*bitmap,
            -GetScrollPos(wxHORIZONTAL) * 20,
            -GetScrollPos(wxVERTICAL) * 20, false);
    }
}

void wxImagePanel::OnPaint(wxPaintEvent &evt) {
    wxPaintDC dc(this);
    render(dc);
}

void wxImagePanel::OnScroll(wxScrollWinEvent &evt) {
    paintNow();
    evt.Skip();
}
