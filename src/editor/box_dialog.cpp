#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

box_dialog::box_dialog(wxWindow *parent, layout_box &box) :
    wxDialog(parent, wxID_ANY, "Opzioni rettangolo", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), box(box)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    auto addLabelAndCtrl = [&](const wxString &labelText, wxWindow *ctrl, int proportion = 0) {
        wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText *label = new wxStaticText(this, wxID_ANY, labelText, wxDefaultPosition, wxSize(60, -1), wxALIGN_RIGHT);
        hsizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

        hsizer->Add(ctrl, 1, wxEXPAND | wxLEFT, 5);

        sizer->Add(hsizer, proportion, wxEXPAND | wxALL, 5);
    };
    
    m_box_name = new wxTextCtrl(this, wxID_ANY, box.name);
    addLabelAndCtrl("Nome:", m_box_name);

    static const wxString box_types[] = {"Singolo elemento", "Elementi multipli", "Elementi per colonna", "Elementi per riga"};
    m_box_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(box_types), box_types);
    m_box_type->SetSelection(box.type);
    addLabelAndCtrl("Metodo lettura:", m_box_type);

    m_box_script = new wxTextCtrl(this, wxID_ANY, box.script, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    addLabelAndCtrl("Script:", m_box_script, 1);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    sizer->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton *okButton = new wxButton(this, wxID_OK, "OK");
    okCancelSizer->Add(okButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Annulla");
    okCancelSizer->Add(cancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *helpButton = new wxButton(this, wxID_HELP, "Aiuto");
    okCancelSizer->Add(helpButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    sizer->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizerAndFit(sizer);
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
    EVT_BUTTON(wxID_HELP, box_dialog::OnClickHelp)
END_EVENT_TABLE()

bool box_dialog::validateData() {
    if (m_box_name->GetValue().IsEmpty()) return false;
    if (m_box_script->GetValue().IsEmpty()) return false;
    return true;
}

void box_dialog::OnOK(wxCommandEvent &evt) {
    if (validateData()) {
        box.name = m_box_name->GetValue();
        box.type = static_cast<box_type>(m_box_type->GetSelection());
        box.script = m_box_script->GetValue();
        evt.Skip();
    } else {
        wxBell();
    }
}

void box_dialog::OnClickHelp(wxCommandEvent &evt) {
    wxMessageBox(
        "Inserire nel campo script gli identificatori dei vari elementi nel rettangolo, uno per riga.\n"
        "Ogni identificatore deve essere una stringa unica e non deve iniziare per numero.\n"
        "I valori numerici sono identificati da un %, per esempio %totale_fattura\n"
        "I valori da saltare sono identificati da un #, per esempio #unita\n"
        "Consultare la documentazione per funzioni avanzate",
        "Aiuto elementi", wxICON_INFORMATION);
}