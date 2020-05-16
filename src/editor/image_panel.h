#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/image.h>
#include <wx/scrolwin.h>

class wxImagePanel : public wxScrolledWindow {
public:
    wxImagePanel(wxWindow *parent);
    ~wxImagePanel();

    wxImage *getImage() {
        return image;
    }
    void setImage(const wxImage &new_image);

    void rescale(float factor, wxImageResizeQuality quality = wxIMAGE_QUALITY_NORMAL);

protected:
    virtual bool render(wxDC &dc);
    
    int scrollx = 0;
    int scrolly = 0;

    float scaled_width;
    float scaled_height;

    float scale = 0.5f;

private:
    wxImage *image = nullptr;
    wxImage scaled_image;
    
    void OnPaint(wxPaintEvent &evt);
    
    DECLARE_EVENT_TABLE()
};

#endif