#ifndef __COMPILE_ERROR_DIAG_H__
#define __COMPILE_ERROR_DIAG_H__

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

class CompileErrorDialog : public wxDialog {
public:
    CompileErrorDialog(wxWindow *parent, const wxString &message)
        : wxDialog(parent, wxID_ANY, "Errore", wxDefaultPosition, wxSize(500, 200), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

        sizer->Add(new wxStaticText(this, wxID_ANY, "Errore in compilazione:"), 0, wxEXPAND | wxALL, 5);

        wxTextCtrl *m_text_ctl = new wxTextCtrl(this, wxID_ANY, message, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_DONTWRAP | wxTE_READONLY);
        m_text_ctl->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        sizer->Add(m_text_ctl, 1, wxEXPAND | wxALL, 5);

        sizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALIGN_CENTER | wxALL, 5);
        SetSizer(sizer);
    }
    
    virtual int ShowModal() {
        wxBell();
        return wxDialog::ShowModal();
    }
};

#endif