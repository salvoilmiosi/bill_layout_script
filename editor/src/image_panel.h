#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/image.h>
#include <wx/scrolwin.h>
#include <wx/bitmap.h>

class wxImagePanel : public wxScrolledCanvas {
public:
    wxImagePanel(wxWindow *parent);

    void setImage(const wxImage &new_image);

    void rescale(float factor, wxImageResizeQuality quality = wxIMAGE_QUALITY_NORMAL);

    double scaled_width() {
        return scaled_image.GetWidth();
    }

    double scaled_height() {
        return scaled_image.GetHeight();
    }

protected:
    float m_scale = 0.5f;

protected:
    wxImage raw_image;
    wxBitmap scaled_image;
    
    virtual void render(wxDC &dc);

private:
    virtual void OnDraw(wxDC &dc) override;
};

#endif