#ifndef __FRAME_H__
#define __FRAME_H__

#include <wx/image.h>
#include <wx/scrolwin.h>

class wxImagePanel : public wxScrolledWindow {
public:
    wxImagePanel(wxWindow *parent);
    ~wxImagePanel();

    void setImage(const wxImage &new_image);

    void paintNow(bool clear = false);

    void rescale(float factor);

    int getScrollX() {
        return scrollx;
    }

    int getScrollY() {
        return scrolly;
    }
    
protected:
    virtual void OnMouseDown(wxMouseEvent &evt) { evt.Skip(); };
    virtual void OnMouseUp(wxMouseEvent &evt) { evt.Skip(); };
    virtual void OnMouseMove(wxMouseEvent &evt) { evt.Skip(); };
    virtual bool render(wxDC &dc, bool clear = false);

private:
    virtual void OnPaint(wxPaintEvent &evt);
    
    DECLARE_EVENT_TABLE()

private:
    wxImage *image = nullptr;
    wxImage scaled_image;

    int scrollx = 0;
    int scrolly = 0;

    float scale = 0.5f;
};

#endif