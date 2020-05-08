#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/stattext.h>

box_dialog::box_dialog(wxWindow *parent, layout_box &box) : wxDialog(parent, wxID_ANY, "Opzioni rettangolo"), box(box) {
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *name_label = new wxStaticText(this, wxID_ANY, "Nome del rettangolo:");
    sizer->Add(name_label, 0, wxEXPAND | wxALL, 5);

    m_box_name = new wxTextCtrl(this, wxID_ANY);
    sizer->Add(m_box_name, 0, wxEXPAND | wxALL, 5);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    sizer->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton *okButton = new wxButton(this, wxID_OK, "OK");
    okCancelSizer->Add(okButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Annulla");
    okCancelSizer->Add(cancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    sizer->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizer(sizer);
    Fit();
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
END_EVENT_TABLE()

bool box_dialog::validateAndUpdateBox() {
    if (! m_box_name->GetValue().IsEmpty()) {
        box.name = m_box_name->GetValue();
        return true;
    }
    return false;
}

void box_dialog::OnOK(wxCommandEvent &evt) {
    if (validateAndUpdateBox()) {
        evt.Skip();
    } else {
        wxBell();
    }
}