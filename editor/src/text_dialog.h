#ifndef __TEXT_DIALOG_H__
#define __TEXT_DIALOG_H__

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>

class TextDialog : public wxDialog {
public:
    TextDialog(wxWindow *parent, const wxString &title)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(500, 200), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

        m_text_ctl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_DONTWRAP | wxTE_READONLY);
        m_text_ctl->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        sizer->Add(m_text_ctl, 1, wxEXPAND | wxALL, 5);

        sizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALIGN_CENTER | wxALL, 5);
        SetSizer(sizer);
    }

    void ShowText(const wxString &message) {
        m_text_ctl->SetValue(message);
        Show();
    }

private:
    wxTextCtrl *m_text_ctl;
};

#endif