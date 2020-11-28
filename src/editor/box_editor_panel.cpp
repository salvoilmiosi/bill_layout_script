#include "box_editor_panel.h"

#include "box_dialog.h"

#include <wx/textdlg.h>

BEGIN_EVENT_TABLE(box_editor_panel, wxImagePanel)
    EVT_LEFT_DOWN(box_editor_panel::OnMouseDown)
    EVT_LEFT_UP(box_editor_panel::OnMouseUp)
    EVT_LEFT_DCLICK(box_editor_panel::OnDoubleClick)
    EVT_MOTION(box_editor_panel::OnMouseMove)
    EVT_KEY_DOWN(box_editor_panel::OnKeyDown)
    EVT_KEY_UP(box_editor_panel::OnKeyUp)
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

bool box_editor_panel::render(wxDC &dc) {
    auto make_layout_rect = [&](auto &box) {
        wxRect rect;
        rect.x = box->x * scaled_width - scrollx;
        rect.y = box->y * scaled_height - scrolly;
        rect.width = box->w * scaled_width;
        rect.height = box->h * scaled_height;
        return rect;
    };

    if (wxImagePanel::render(dc)) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        for (auto &box : app->layout) {
            if (box->page == app->getSelectedPage()) {
                if (box->selected) {
                    dc.SetPen(*wxBLACK_DASHED_PEN);
                } else {
                    dc.SetPen(*wxBLACK_PEN);
                }
                dc.DrawRectangle(make_layout_rect(box));
            }
        }

        switch (selected_tool) {
        case TOOL_NEWBOX:
            if (mouseIsDown) {
                dc.SetPen(*wxBLACK_PEN);
                dc.DrawRectangle(make_rect(start_pt, end_pt));
            }
            break;
        }
        return true;
    }
    return false;
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    float xx = (evt.GetX() + scrollx) / scaled_width;
    float yy = (evt.GetY() + scrolly) / scaled_height;
    if (getImage() && !mouseIsDown) {
        switch (selected_tool) {
        case TOOL_SELECT:
            selected_box = getBoxAt(app->layout, xx, yy, app->getSelectedPage());
            if (selected_box) {
                startx = selected_box->x;
                starty = selected_box->y;
                app->selectBox(selected_box);
                mouseIsDown = true;
                start_pt = evt.GetPosition();
            } else {
                app->selectBox(nullptr);
            }
            break;
        case TOOL_NEWBOX:
            mouseIsDown = true;
            start_pt = evt.GetPosition();
            break;
        case TOOL_DELETEBOX:
        {
            auto it = getBoxAt(app->layout, xx, yy, app->getSelectedPage());
            if (it) {
                app->layout.erase(std::find(app->layout.begin(), app->layout.end(), it));
                app->updateLayout();
                Refresh();
            }
            break;
        }
        case TOOL_RESIZE:
        {
            auto node = getBoxResizeNode(app->layout, xx, yy, app->getSelectedPage(), scaled_width, scaled_height);
            selected_box = node.first;
            if (node.second) {
                resize_node = node.second;
                app->selectBox(selected_box);
                mouseIsDown = true;
                start_pt = evt.GetPosition();
            } else {
                app->selectBox(nullptr);
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
                if (selected_box && start_pt != end_pt) {
                    app->updateLayout();
                }
                break;
            case TOOL_NEWBOX:
            {
                wxTextEntryDialog diag(this, "Inserisci nome:", "Nome Rettangolo");
                diag.SetTextValidator(wxFILTER_EMPTY);
                if (diag.ShowModal() == wxID_OK) {
                    wxRect rect = make_rect(start_pt, end_pt);
                    auto &box = insertAfterSelected(app->layout);
                    box->name = diag.GetValue();
                    box->x = (rect.x + scrollx) / scaled_width;
                    box->y = (rect.y + scrolly) / scaled_height;
                    box->w = rect.width / scaled_width;
                    box->h = rect.height / scaled_height;
                    box->page = app->getSelectedPage();
                    app->updateLayout();
                    app->selectBox(box);
                    new box_dialog(app, *box);
                }
                break;
            }
            case TOOL_DELETEBOX:
                break;
            case TOOL_RESIZE:
                if (selected_box) {
                    if (start_pt != end_pt) {
                        if (selected_box->w < 0) {
                            selected_box->w = -selected_box->w;
                            selected_box->x -= selected_box->w;
                        }
                        if (selected_box->h < 0) {
                            selected_box->h = -selected_box->h;
                            selected_box->y -= selected_box->h;
                        }
                        app->updateLayout();
                    }
                }
                OnMouseMove(evt); // changes cursor
                break;
            }
        }

        Refresh();
    }
    evt.Skip();
}

void box_editor_panel::OnDoubleClick(wxMouseEvent &evt) {
    switch (selected_tool) {
    case TOOL_SELECT:
        if (selected_box) {
            new box_dialog(app, *selected_box);
        }
    default:
        break;
    }
}

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    float xx = (evt.GetX() + scrollx) / scaled_width;
    float yy = (evt.GetY() + scrolly) / scaled_height;
    if (mouseIsDown) {
        end_pt = evt.GetPosition();
        switch (selected_tool) {
        case TOOL_SELECT:
        {
            float dx = (end_pt.x - start_pt.x) / scaled_width;
            float dy = (end_pt.y - start_pt.y) / scaled_height;
            selected_box->x = startx + dx;
            selected_box->y = starty + dy;
            break;
        }
        case TOOL_RESIZE:
        {
            if (resize_node & RESIZE_TOP) {
                selected_box->h = selected_box->y + selected_box->h - yy;
                selected_box->y = yy;
            } else if (resize_node & RESIZE_BOTTOM) {
                selected_box->h = yy - selected_box->y;
            }
            if (resize_node & RESIZE_LEFT) {
                selected_box->w = selected_box->x + selected_box->w - xx;
                selected_box->x = xx;
            } else if (resize_node & RESIZE_RIGHT) {
                selected_box->w = xx - selected_box->x;
            }
            break;
        }
        default:
            break;
        }
        Refresh();
        if (evt.Leaving()) {
            OnMouseUp(evt);
        }
    } else {
        switch (selected_tool) {
        case TOOL_RESIZE:
            auto node = getBoxResizeNode(app->layout, xx, yy, app->getSelectedPage(), scaled_width, scaled_height);
            switch (node.second) {
            case RESIZE_LEFT:
            case RESIZE_RIGHT:
                SetCursor(wxStockCursor::wxCURSOR_SIZEWE);
                break;
            case RESIZE_TOP:
            case RESIZE_BOTTOM:
                SetCursor(wxStockCursor::wxCURSOR_SIZENS);
                break;
            case RESIZE_LEFT | RESIZE_TOP:
            case RESIZE_RIGHT | RESIZE_BOTTOM:
                SetCursor(wxStockCursor::wxCURSOR_SIZENWSE);
                break;
            case RESIZE_RIGHT | RESIZE_TOP:
            case RESIZE_LEFT | RESIZE_BOTTOM:
                SetCursor(wxStockCursor::wxCURSOR_SIZENESW);
                break;
            default:
                SetCursor(*wxSTANDARD_CURSOR);
                break;
            }
        }
    }
    evt.Skip();
}

void box_editor_panel::OnKeyDown(wxKeyEvent &evt) {
    constexpr float MOVE_AMT = 5.f;
    if (selected_box) {
        switch (evt.GetKeyCode()) {
            case WXK_LEFT: selected_box->x -= MOVE_AMT / scaled_width; Refresh(); break;
            case WXK_RIGHT: selected_box->x += MOVE_AMT / scaled_width; Refresh(); break;
            case WXK_UP: selected_box->y -= MOVE_AMT / scaled_height; Refresh(); break;
            case WXK_DOWN: selected_box->y += MOVE_AMT / scaled_height; Refresh(); break;
        }
    }
}

void box_editor_panel::OnKeyUp(wxKeyEvent &evt) {
    if (selected_box) {
        switch (evt.GetKeyCode()) {
        case WXK_LEFT:
        case WXK_RIGHT:
        case WXK_UP:
        case WXK_DOWN:
            app->updateLayout();
        }
    }
}