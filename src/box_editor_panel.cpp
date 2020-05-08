#include "box_editor_panel.h"

#include "main.h"

box_editor_panel::box_editor_panel(wxWindow *parent, MainApp *app) : wxImagePanel(parent), app(app) {

}

static wxRect make_rect(wxPoint start_pt, wxPoint end_pt) {
    wxRect rect;
    rect.x = wxMin(start_pt.x, end_pt.x);
    rect.y = wxMin(start_pt.y, end_pt.y);
    rect.width = abs(start_pt.x - end_pt.x);
    rect.height = abs(start_pt.y - end_pt.y);
    return rect;
}

bool box_editor_panel::render(wxDC &dc, bool clear) {
    if (wxImagePanel::render(dc, clear)) {
        if (drawing) {
            dc.DrawRectangle(make_rect(start_pt, end_pt));
        }
        return true;
    }
    return false;
}

void box_editor_panel::OnMouseDown(wxMouseEvent &evt) {
    drawing = true;
    start_pt = evt.GetPosition();
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
        xpdf::rect pdf_rect;
        pdf_rect.x = (float) rect.x / GetVirtualSize().GetWidth();
        pdf_rect.y = (float) rect.y / GetVirtualSize().GetHeight();
        pdf_rect.w = (float) rect.width / GetVirtualSize().GetWidth();
        pdf_rect.h = (float) rect.height / GetVirtualSize().GetHeight();
        pdf_rect.page = app->selected_page;

        std::string output = xpdf::pdf_to_text(app->app_path, app->pdf_filename, app->info, pdf_rect);
        wxMessageBox(trim(output));
    }
    evt.Skip();
}

void box_editor_panel::OnMouseMove(wxMouseEvent &evt) {
    if (drawing) {
        end_pt = evt.GetPosition();
        paintNow();
    }
    evt.Skip();
}