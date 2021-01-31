#ifndef __BOX_EDITOR_PANEL_H__
#define __BOX_EDITOR_PANEL_H__

#include "image_panel.h"
#include "editor.h"
#include "text_dialog.h"

#include "layout_ext.h"

class box_editor_panel : public wxImagePanel {
public:
    box_editor_panel(wxWindow *parent, class frame_editor *app);

    void setSelectedTool(int tool) {
        selected_tool = tool;
    }

    void setSelectedBox(const box_ptr &box) {
        selected_box = box;
    }

protected:
    bool render(wxDC &dc) override;
    void OnMouseDown(wxMouseEvent &evt);
    void OnMouseUp(wxMouseEvent &evt);
    void OnDoubleClick(wxMouseEvent &evt);
    void OnMouseMove(wxMouseEvent &evt);
    void OnKeyDown(wxKeyEvent &evt);
    void OnKeyUp(wxKeyEvent &evt);

private:
    class frame_editor *app;

    TextDialog *info_dialog;

    wxPoint start_pt, end_pt;
    box_ptr selected_box;
    char resize_node = 0;
    float startx, starty;
    bool mouseIsDown = false;

    int selected_tool = TOOL_SELECT;

private:
    DECLARE_EVENT_TABLE()
};

#endif