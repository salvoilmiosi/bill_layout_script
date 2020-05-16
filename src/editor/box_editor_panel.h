#ifndef __BOX_EDITOR_PANEL_H__
#define __BOX_EDITOR_PANEL_H__

#include "image_panel.h"

#include "editor.h"

class box_editor_panel : public wxImagePanel {
public:
    box_editor_panel(wxWindow *parent, class frame_editor *app);

    void setSelectedTool(int tool) {
        selected_tool = tool;
    }

protected:
    bool render(wxDC &dc) override;
    void OnMouseDown(wxMouseEvent &evt);
    void OnMouseUp(wxMouseEvent &evt);
    void OnDoubleClick(wxMouseEvent &evt);
    void OnMouseMove(wxMouseEvent &evt);

private:
    class frame_editor *app;

    wxPoint start_pt, end_pt;
    box_reference selected_box;
    char resize_node = 0;
    float startx, starty;
    bool mouseIsDown = false;

    int selected_tool = TOOL_SELECT;

private:
    DECLARE_EVENT_TABLE()
};

#endif