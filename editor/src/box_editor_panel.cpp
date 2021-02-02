#include "box_editor_panel.h"

#include "box_dialog.h"

#include <wx/choicdlg.h>
#include <wx/dcbuffer.h>

BEGIN_EVENT_TABLE(box_editor_panel, wxImagePanel)
    EVT_LEFT_DOWN(box_editor_panel::OnMouseDown)
    EVT_LEFT_UP(box_editor_panel::OnMouseUp)
    EVT_LEFT_DCLICK(box_editor_panel::OnDoubleClick)
    EVT_MOTION(box_editor_panel::OnMouseMove)
    EVT_KEY_DOWN(box_editor_panel::OnKeyDown)
    EVT_KEY_UP(box_editor_panel::OnKeyUp)
END_EVENT_TABLE()

box_editor_panel::box_editor_panel(wxWindow *parent, frame_editor *app) : wxImagePanel(parent), app(app) {
    info_dialog = new TextDialog(this, "Test Lettura Rettangolo");
}

static void clamp_rect(pdf_rect &rect) {
    if (rect.x < 0.f) {
        rect.w += rect.x;
        rect.x = 0.f;
    }
    if (rect.x + rect.w > 1.f) {
        rect.w = 1.f - rect.x;
    }
    if (rect.y < 0.f) {
        rect.h += rect.y;
        rect.y = 0.f;
    }
    if (rect.y + rect.h > 1.f) {
        rect.h = 1.f - rect.y;
    }
}

void box_editor_panel::render(wxDC &dc) {
    wxImagePanel::render(dc);

    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    for (auto &box : app->layout.m_boxes) {
        if (box->page == app->getSelectedPage()) {
            if (box->selected) {
                dc.SetPen(*wxBLACK_DASHED_PEN);
            } else {
                dc.SetPen(*wxBLACK_PEN);
            }
            pdf_rect r = *box;
            clamp_rect(r);
            dc.DrawRectangle(
                r.x * scaled_width(),
                r.y * scaled_height(),
                r.w * scaled_width(),
                r.h * scaled_height()
            );
        }
    }

    switch (selected_tool) {
    case TOOL_NEWBOX:
    case TOOL_TEST:
        if (mouseIsDown) {
            dc.SetPen(*wxBLACK_PEN);
            dc.DrawRectangle(wxRect (
                wxPoint(start_pt.x * scaled_width(), start_pt.y * scaled_height()),
                wxPoint(end_pt.x * scaled_width(), end_pt.y * scaled_height())
            ));
        }
        break;
    }
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    start_pt = screen_to_layout(evt.GetPosition());
    if (raw_image.IsOk() && !mouseIsDown) {
        switch (selected_tool) {
        case TOOL_SELECT:
            selected_box = getBoxAt(app->layout, start_pt.x, start_pt.y, app->getSelectedPage());
            if (selected_box) {
                dragging_offset.x = selected_box->x - start_pt.x;
                dragging_offset.y = selected_box->y - start_pt.y;
                app->selectBox(selected_box);
                mouseIsDown = true;
            } else {
                app->selectBox(nullptr);
            }
            break;
        case TOOL_NEWBOX:
        case TOOL_TEST:
            mouseIsDown = true;
            break;
        case TOOL_DELETEBOX: {
            auto it = getBoxAt(app->layout, start_pt.x, start_pt.y, app->getSelectedPage());
            if (box_dialog::closeDialog(it)) {
                app->layout.m_boxes.erase(std::find(app->layout.m_boxes.begin(), app->layout.m_boxes.end(), it));
                app->updateLayout();
                Refresh();
            }
            break;
        }
        case TOOL_RESIZE: {
            auto node = getBoxResizeNode(app->layout, start_pt.x, start_pt.y, app->getSelectedPage(), scaled_width(), scaled_height());
            selected_box = node.first;
            if (node.second) {
                resize_node = node.second;
                app->selectBox(selected_box);
                mouseIsDown = true;
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

        end_pt = screen_to_layout(evt.GetPosition());
        if (end_pt != start_pt) {
            switch (selected_tool) {
            case TOOL_SELECT:
                if (selected_box && start_pt != end_pt) {
                    clamp_rect(*selected_box);
                    app->updateLayout();
                }
                break;
            case TOOL_NEWBOX: {
                auto &box = insertAfterSelected(app->layout);
                box->x = std::min(start_pt.x, end_pt.x);
                box->y = std::min(start_pt.y, end_pt.y);
                box->w = std::abs(start_pt.x - end_pt.x);
                box->h = std::abs(start_pt.y - end_pt.y);
                clamp_rect(*box);
                box->page = app->getSelectedPage();
                app->updateLayout();
                app->selectBox(box);
                box_dialog::openDialog(app, box);
                break;
            }
            case TOOL_TEST: {
                wxArrayString choices;
                for (auto &str : read_mode_labels) {
                    choices.push_back(str);
                }
                wxSingleChoiceDialog diag(this, L"Modalità di lettura:", "Test Lettura Rettangolo", choices);
                if (diag.ShowModal() == wxID_OK) {
                    pdf_rect box;
                    box.x = std::min(start_pt.x, end_pt.x);
                    box.y = std::min(start_pt.y, end_pt.y);
                    box.w = std::abs(start_pt.x - end_pt.x);
                    box.h = std::abs(start_pt.y - end_pt.y);
                    box.page = app->getSelectedPage();
                    box.mode = static_cast<read_mode>(diag.GetSelection());
                    std::string text = app->getPdfDocument().get_text(box);
                    info_dialog->ShowText(wxString(text.c_str(), wxConvUTF8));
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
            box_dialog::openDialog(app, selected_box);
        }
    default:
        break;
    }
}

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    end_pt = screen_to_layout(evt.GetPosition());

    if (mouseIsDown) {
        switch (selected_tool) {
        case TOOL_SELECT:
            selected_box->x = dragging_offset.x + end_pt.x;
            selected_box->y = dragging_offset.y + end_pt.y;
            break;
        case TOOL_RESIZE: {
            if (resize_node & RESIZE_TOP) {
                selected_box->h = selected_box->y + selected_box->h - end_pt.y;
                selected_box->y = end_pt.y;
            } else if (resize_node & RESIZE_BOTTOM) {
                selected_box->h = end_pt.y - selected_box->y;
            }
            if (resize_node & RESIZE_LEFT) {
                selected_box->w = selected_box->x + selected_box->w - end_pt.x;
                selected_box->x = end_pt.x;
            } else if (resize_node & RESIZE_RIGHT) {
                selected_box->w = end_pt.x - selected_box->x;
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
            auto node = getBoxResizeNode(app->layout, end_pt.x, end_pt.y, app->getSelectedPage(), scaled_width(), scaled_height());
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
            case WXK_LEFT: selected_box->x -= MOVE_AMT / scaled_width(); Refresh(); break;
            case WXK_RIGHT: selected_box->x += MOVE_AMT / scaled_width(); Refresh(); break;
            case WXK_UP: selected_box->y -= MOVE_AMT / scaled_height(); Refresh(); break;
            case WXK_DOWN: selected_box->y += MOVE_AMT / scaled_height(); Refresh(); break;
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