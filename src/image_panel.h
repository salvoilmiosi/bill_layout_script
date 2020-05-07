#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/wx.h>

class wxImagePanel : public wxScrolledWindow {
public:
    wxImagePanel(wxWindow *parent);
    ~wxImagePanel();

    void setImage(const wxImage &image);

    void paintNow();

private:
    void OnPaint(wxPaintEvent &evt);
    void OnScroll(wxScrollWinEvent &evt);
    void render(wxDC &dc);

private:
    wxBitmap *bitmap = nullptr;

    DECLARE_EVENT_TABLE()
};

#endif