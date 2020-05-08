#include "box_editor_panel.h"

#include "main.h"
#include "box_dialog.h"

box_editor_panel::box_editor_panel(wxWindow *parent, MainApp *app) : wxImagePanel(parent), app(app) {

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
            if (box.page == app->selected_page) {
                if (box.selected) {
                    dc.SetPen(*wxBLACK_DASHED_PEN);
                } else {
                    dc.SetPen(*wxBLACK_PEN);
                }
                dc.DrawRectangle(make_layout_rect(box));
            }
        }

        switch (app->selected_tool) {
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

std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    str.erase(0, str.find_first_not_of(chars));
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    float xx = (evt.GetX() + getScrollX()) / getScaledWidth();
    float yy = (evt.GetY() + getScrollY()) / getScaledHeight();
    if (getImage() && !mouseIsDown) {
        switch (app->selected_tool) {
        case TOOL_SELECT:
            selected_box = app->layout.getBoxUnder(xx, yy, app->selected_page);
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
            auto it = app->layout.getBoxUnder(xx, yy, app->selected_page);
            if (it != app->layout.boxes.end()) {
                app->layout.boxes.erase(it);
                app->updateBoxList();
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
            switch (app->selected_tool) {
            case TOOL_SELECT:
                break;
            case TOOL_NEWBOX:
            {
                wxRect rect = make_rect(start_pt, end_pt);
                layout_box box;
                box.x = (rect.x + getScrollX()) / getScaledWidth();
                box.y = (rect.y + getScrollY()) / getScaledHeight();
                box.w = rect.width / getScaledWidth();
                box.h = rect.height / getScaledHeight();
                box.page = app->selected_page;

                box_dialog diag(this, box);
                if (diag.ShowModal() == wxID_OK) {
                    app->layout.boxes.push_back(box);
                    app->updateBoxList();
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

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    if (mouseIsDown) {
        end_pt = evt.GetPosition();
        switch (app->selected_tool) {
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