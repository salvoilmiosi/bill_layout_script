#include "box_editor_panel.h"

#include "box_dialog.h"

BEGIN_EVENT_TABLE(box_editor_panel, wxImagePanel)
    EVT_LEFT_DOWN(box_editor_panel::OnMouseDown)
    EVT_LEFT_UP(box_editor_panel::OnMouseUp)
    EVT_LEFT_DCLICK(box_editor_panel::OnDoubleClick)
    EVT_MOTION(box_editor_panel::OnMouseMove)
END_EVENT_TABLE()

box_editor_panel::box_editor_panel(wxWindow *parent, frame_editor *app) : wxImagePanel(parent), app(app) {

}

static auto make_rect = [](wxPoint start_pt, wxPoint end_pt) {
    wxRect rect;
    rect.x = wxMin(start_pt.x, end_pt.x);
    rect.y = wxMin(start_pt.y, end_pt.y);
    rect.width = abs(start_pt.x - end_pt.x);
    rect.height = abs(start_pt.y - end_pt.y);
    return rect;
};

bool box_editor_panel::render(wxDC &dc, bool clear) {
    auto make_layout_rect = [&](auto &box) {
        wxRect rect;
        rect.x = box.x * getScaledWidth() - getScrollX();
        rect.y = box.y * getScaledHeight() - getScrollY();
        rect.width = box.w * getScaledWidth();
        rect.height = box.h * getScaledHeight();
        return rect;
    };

    if (wxImagePanel::render(dc, clear)) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        for (auto &box : app->layout.boxes) {
            if (box.page == app->getSelectedPage()) {
                if (box.selected) {
                    dc.SetPen(*wxBLACK_DASHED_PEN);
                } else {
                    dc.SetPen(*wxBLACK_PEN);
                }
                dc.DrawRectangle(make_layout_rect(box));
            }
        }

        switch (selected_tool) {
        case TOOL_SELECT:
            break;
        case TOOL_NEWBOX:
            if (mouseIsDown) {
                dc.SetPen(*wxBLACK_DASHED_PEN);
                dc.DrawRectangle(make_rect(start_pt, end_pt));
            }
            break;
        case TOOL_DELETEBOX:
            break;
        }
        return true;
    }
    return false;
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    float xx = (evt.GetX() + getScrollX()) / getScaledWidth();
    float yy = (evt.GetY() + getScrollY()) / getScaledHeight();
    if (getImage() && !mouseIsDown) {
        switch (selected_tool) {
        case TOOL_SELECT:
            selected_box = app->layout.getBoxAt(xx, yy, app->getSelectedPage());
            if (selected_box != app->layout.boxes.end()) {
                startx = selected_box->x;
                starty = selected_box->y;
                app->selectBox(selected_box - app->layout.boxes.begin());
                mouseIsDown = true;
                start_pt = evt.GetPosition();
            } else {
                app->selectBox(-1);
            }
            break;
        case TOOL_NEWBOX:
            mouseIsDown = true;
            start_pt = evt.GetPosition();
            break;
        case TOOL_DELETEBOX:
        {
            auto it = app->layout.getBoxAt(xx, yy, app->getSelectedPage());
            if (it != app->layout.boxes.end()) {
                app->layout.boxes.erase(it);
                app->updateLayout();
                paintNow();
            }
            break;
        }
        }
    }
    evt.Skip();
}

void box_editor_panel::OnMouseUp(wxMouseEvent &evt) {
    if (mouseIsDown) {
        mouseIsDown = false;

        end_pt = evt.GetPosition();
        if (end_pt != start_pt) {
            switch (selected_tool) {
            case TOOL_SELECT:
                if (selected_box != app->layout.boxes.end()) {
                    if (start_pt != end_pt) {
                        app->updateLayout();
                    }
                }
                break;
            case TOOL_NEWBOX:
            {
                wxRect rect = make_rect(start_pt, end_pt);
                layout_box box;
                box.x = (rect.x + getScrollX()) / getScaledWidth();
                box.y = (rect.y + getScrollY()) / getScaledHeight();
                box.w = rect.width / getScaledWidth();
                box.h = rect.height / getScaledHeight();
                box.page = app->getSelectedPage();

                box_dialog diag(this, box);
                if (diag.ShowModal() == wxID_OK) {
                    app->layout.boxes.push_back(box);
                    app->updateLayout();
                    app->selectBox(app->layout.boxes.size() - 1);
                }
                break;
            }
            case TOOL_DELETEBOX:
                break;
            }
        }

        paintNow(true);
    }
    evt.Skip();
}

void box_editor_panel::OnDoubleClick(wxMouseEvent &evt) {
    switch (selected_tool) {
    case TOOL_SELECT:
        if (selected_box != app->layout.boxes.end()) {
            box_dialog diag(this, *selected_box);
            if (diag.ShowModal() == wxID_OK) {
                app->updateLayout();
            }
        }
    case TOOL_NEWBOX:
        break;
    case TOOL_DELETEBOX:
        break;
    }
}

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    if (mouseIsDown) {
        end_pt = evt.GetPosition();
        switch (selected_tool) {
        case TOOL_SELECT:
        {
            float dx = (end_pt.x - start_pt.x) / getScaledWidth();
            float dy = (end_pt.y - start_pt.y) / getScaledHeight();
            selected_box->x = startx + dx;
            selected_box->y = starty + dy;
            break;
        }
        case TOOL_NEWBOX:
            break;
        case TOOL_DELETEBOX:
            break;
        }
        paintNow();
        if (evt.Leaving()) {
            OnMouseUp(evt);
        }
    }
    evt.Skip();
}