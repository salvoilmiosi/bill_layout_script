#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/image.h>
#include <wx/scrolwin.h>

class wxImagePanel : public wxScrolledWindow {
public:
    wxImagePanel(wxWindow *parent);
    ~wxImagePanel();

    void setImage(const wxImage &new_image);

    void paintNow();

    void rescale(float factor);

private:
    void OnPaint(wxPaintEvent &evt);
    void render(wxDC &dc);

    DECLARE_EVENT_TABLE()

private:
    wxImage *image = nullptr;
    wxImage scaled_image;

    float scale = 0.5f;
};

#endif