#include "layout_options_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>

BEGIN_EVENT_TABLE(LayoutOptionsDialog, wxDialog)
    EVT_BUTTON(wxID_OK, LayoutOptionsDialog::OnOK)
END_EVENT_TABLE()

LayoutOptionsDialog::LayoutOptionsDialog(wxWindow *parent, layout_box_list *layout) :
    wxDialog(parent, wxID_ANY, "Cambia Opzioni Di Layout", wxDefaultPosition, wxSize(400, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_layout(layout)
{
    wxArrayString choices;

    choices.push_back("(Seleziona una lingua)");
    lang_codes.push_back(0);
    for (int lang = wxLANGUAGE_UNKNOWN + 1; lang != wxLANGUAGE_USER_DEFINED; ++lang) {
        if (wxLocale::IsAvailable(lang)) {
            choices.push_back(wxLocale::GetLanguageName(lang));
            lang_codes.push_back(intl::language(lang));
        }
    }

    m_lang_list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices, wxLB_SINGLE);

    intl::language lang = m_layout->language_code;
    if (!intl::valid_language(lang)) {
        lang = intl::system_language();
    }
    auto selected = std::ranges::find(lang_codes, lang);
    if (selected != lang_codes.end()) {
        m_lang_list->SetSelection(selected - lang_codes.begin());
    }

    m_setlayout_box = new wxCheckBox(this, wxID_ANY, "Carica in Auto Layout");
    m_setlayout_box->SetValue(m_layout->setlayout);

    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    top_level->Add(m_lang_list, 1, wxEXPAND | wxALL, 5);

    top_level->Add(m_setlayout_box, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer *ok_cancel_sizer = new wxBoxSizer(wxHORIZONTAL);

    ok_cancel_sizer->Add(new wxButton(this, wxID_OK, "OK", wxDefaultPosition, wxSize(100, -1)), 0, wxALL, 5);
    ok_cancel_sizer->Add(new wxButton(this, wxID_CANCEL, "Annulla", wxDefaultPosition, wxSize(100, -1)), 0, wxALL, 5);

    top_level->Add(ok_cancel_sizer, 0, wxALIGN_CENTER_HORIZONTAL);

    SetSizerAndFit(top_level);
}

void LayoutOptionsDialog::OnOK(wxCommandEvent &evt) {
    m_layout->language_code = lang_codes[m_lang_list->GetSelection()];
    m_layout->setlayout = m_setlayout_box->GetValue();
    evt.Skip();
}