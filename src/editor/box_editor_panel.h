#ifndef __BOX_EDITOR_PANEL_H__
#define __BOX_EDITOR_PANEL_H__

#include "image_panel.h"

#include "../shared/layout.h"

class box_editor_panel : public wxImagePanel {
public:
    box_editor_panel(wxWindow *parent, class MainApp *app);

private:
    layout_box *getBoxUnder(int x, int y);

protected:
    bool render(wxDC &dc, bool clear = false) override;
    void OnMouseDown(wxMouseEvent &evt);
    void OnMouseUp(wxMouseEvent &evt);
    void OnDoubleClick(wxMouseEvent &evt);
    void OnMouseMove(wxMouseEvent &evt);

private:
    class MainApp *app;

    wxPoint start_pt, end_pt;
    std::vector<layout_box>::iterator selected_box;
    float startx, starty;
    bool mouseIsDown = false;

private:
    DECLARE_EVENT_TABLE()
};

#endif