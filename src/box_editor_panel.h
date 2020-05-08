#ifndef __BOX_EDITOR_PANEL_H__
#define __BOX_EDITOR_PANEL_H__

#include "image_panel.h"

class box_editor_panel : public wxImagePanel {
public:
    box_editor_panel(wxWindow *parent, class MainApp *app);

protected:
    bool render(wxDC &dc, bool clear = false) override;
    void OnMouseDown(wxMouseEvent &evt) override;
    void OnMouseUp(wxMouseEvent &evt) override;
    void OnMouseMove(wxMouseEvent &evt) override;

private:
    class MainApp *app;

    wxPoint start_pt, end_pt;
    bool drawing = false;
};

#endif