#include "box_editor_panel.h"

#include "main.h"

#include <wx/textdlg.h>

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
    auto make_layout_rect = [&](box_layout &box) {
        wxRect rect;
        rect.x = box.x * getScaledWidth() - getScrollX();
        rect.y = box.y * getScaledHeight() - getScrollY();
        rect.width = box.w * getScaledWidth();
        rect.height = box.h * getScaledHeight();
        return rect;
    };

    if (wxImagePanel::render(dc, clear)) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        for (auto &box : app->boxes) {
            if (box.page == app->selected_page) {
                dc.DrawRectangle(make_layout_rect(box));
            }
        }

        if (drawing) {
            dc.DrawRectangle(make_rect(start_pt, end_pt));
        }
        return true;
    }
    return false;
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    if (getImage() && !drawing) {
        drawing = true;
        start_pt = evt.GetPosition();
    }
    evt.Skip();
}
 
std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ") {
    str.erase(0, str.find_first_not_of(chars));
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

void box_editor_panel::OnMouseUp(wxMouseEvent &evt) {
    if (drawing) {
        drawing = false;
        wxRect rect = make_rect(start_pt, end_pt);
        box_layout box;
        box.x = (rect.x + getScrollX()) / getScaledWidth();
        box.y = (rect.y + getScrollY()) / getScaledHeight();
        box.w = rect.width / getScaledWidth();
        box.h = rect.height / getScaledHeight();
        box.page = app->selected_page;

        wxTextEntryDialog diag(this, "Inserisci nome");
        if (diag.ShowModal() == wxID_OK) {
            box.name = diag.GetValue().ToStdString();
            box.selected = true;
            if (!box.name.empty()) {
                app->add_box(box);
            }
        }

        paintNow(true);
    }
    evt.Skip();
}

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    if (drawing) {
        end_pt = evt.GetPosition();
        paintNow();
        if (evt.Leaving()) {
            OnMouseUp(evt);
        }
    }
    evt.Skip();
}