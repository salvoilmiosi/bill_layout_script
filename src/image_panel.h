#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/wx.h>

class wxImagePanel : public wxScrolledWindow {
public:
    wxImagePanel(wxWindow *parent);
    ~wxImagePanel();

    void setImage(const wxImage &new_image);

    void paintNow();

    void rescale(float factor);

private:
    void OnPaint(wxPaintEvent &evt);
    void OnScroll(wxScrollWinEvent &evt);
    void render(wxDC &dc);

private:
    wxImage *image = nullptr;
    wxImage scaled_image;

    float scale = 1.f;

    DECLARE_EVENT_TABLE()
};

#endif