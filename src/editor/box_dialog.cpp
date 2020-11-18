#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

#include <json/json.h>

#include "editor.h"
#include "../shared/xpdf.h"
#include "../shared/utils.h"

enum {
    BUTTON_TEST = 10001,
};

template<typename ... Ts>
static void add_to(wxSizer *sizer, wxWindow *first, Ts * ... others) {
    sizer->Add(first, 1, wxEXPAND | wxLEFT, 5);
    if constexpr (sizeof...(others)>0) {
        add_to(sizer, others ... );
    }
}

ReaderOutputDialog::ReaderOutputDialog(wxWindow *parent) :
    wxDialog(parent, wxID_ANY, "Risultato Lettura", wxDefaultPosition, wxSize(400, 400), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    m_text->SetEditable(false);

    sizer->Add(m_text, 1, wxEXPAND | wxALL, 5);
    SetSizer(sizer);
}

void ReaderOutputDialog::ShowText(const wxString &str) {
    m_text->SetValue(str);
    SetPosition(GetParent()->GetPosition() + wxPoint(GetParent()->GetSize().GetX(), 0));
    Show();
}

box_dialog::box_dialog(frame_editor *parent, layout_box &box) :
    wxDialog(parent, wxID_ANY, "Opzioni rettangolo", wxDefaultPosition, wxSize(700, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), box(box), app(parent)
{
    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    auto addLabelAndCtrl = [&](const wxString &labelText, int vprop, wxWindow *ctrl, auto* ... others) {
        wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText *label = new wxStaticText(this, wxID_ANY, labelText, wxDefaultPosition, wxSize(60, -1), wxALIGN_RIGHT);
        hsizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

        add_to(hsizer, ctrl, others ...);

        sizer->Add(hsizer, vprop, wxEXPAND | wxALL, 5);
    };
    
    m_box_name = new wxTextCtrl(this, wxID_ANY, box.name);
    m_box_name->SetValidator(wxTextValidator(wxFILTER_EMPTY));
    addLabelAndCtrl("Nome:", 0, m_box_name);

    static const wxString box_types[] = {"Rettangolo", "Pagina", "File", "Disabilitato"};
    m_box_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(box_types), box_types);
    m_box_type->SetToolTip("Contenuto");
    m_box_type->SetSelection(box.type);

    static const wxString box_modes[] = {"Default", "Layout", "Grezza"};
    m_box_mode = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, std::size(box_modes), box_modes);
    m_box_mode->SetToolTip("Specifica il metodo di lettura");
    m_box_mode->SetSelection(box.mode);

    wxButton *testButton = new wxButton(this, BUTTON_TEST, "Leggi contenuto");
    addLabelAndCtrl("Opzioni:", 0, m_box_type, m_box_mode, testButton);

    m_box_spacers = new wxTextCtrl(this, wxID_ANY, box.spacers);
    addLabelAndCtrl("Spaziatori:", 0, m_box_spacers);

    m_box_goto_label = new wxTextCtrl(this, wxID_ANY, box.goto_label);
    addLabelAndCtrl("Etichetta goto:", 0, m_box_goto_label);

    m_box_script = new wxTextCtrl(this, wxID_ANY, box.script, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    m_box_script->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    addLabelAndCtrl("Script:", 1, m_box_script);

    top_level->Add(sizer, 1, wxEXPAND | wxALL, 5);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    top_level->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton *okButton = new wxButton(this, wxID_OK, "OK");
    okCancelSizer->Add(okButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *cancelButton = new wxButton(this, wxID_CANCEL, "Annulla");
    okCancelSizer->Add(cancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *helpButton = new wxButton(this, wxID_HELP, "Aiuto");
    okCancelSizer->Add(helpButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    top_level->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizer(top_level);

    reader_output = new ReaderOutputDialog(this);
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, box_dialog::OnCancel)
    EVT_BUTTON(wxID_HELP, box_dialog::OnClickHelp)
    EVT_BUTTON(BUTTON_TEST, box_dialog::OnClickTest)
    EVT_CLOSE(box_dialog::OnClose)
END_EVENT_TABLE()

void box_dialog::OnOK(wxCommandEvent &evt) {
    if (Validate()) {
        box.name = m_box_name->GetValue();
        box.type = static_cast<box_type>(m_box_type->GetSelection());
        box.mode = static_cast<read_mode>(m_box_mode->GetSelection());
        box.spacers = m_box_spacers->GetValue();
        box.goto_label = m_box_goto_label->GetValue();
        box.script = m_box_script->GetValue();
        app->updateLayout();
        Destroy();
    }
}

void box_dialog::OnCancel(wxCommandEvent &evt) {
    Destroy();
}

void box_dialog::OnClose(wxCloseEvent &evt) {
    Destroy();
}

void box_dialog::OnClickHelp(wxCommandEvent &evt) {
    reader_output->ShowText(
        "Inserire nel campo script gli identificatori dei vari elementi nel rettangolo, uno per riga.\n"
        "Ogni identificatore deve essere una stringa unica e non deve iniziare per numero.\n"
        "I valori numerici sono identificati da un %, per esempio %totale_fattura\n"
        "I valori da saltare sono identificati da un #, per esempio #unita\n"
        "Consultare la documentazione per funzioni avanzate");
}

void box_dialog::OnClickTest(wxCommandEvent &evt) {
    try {
        pdf_rect copy(dynamic_cast<const pdf_rect &>(box));
        copy.type = static_cast<box_type>(m_box_type->GetSelection());
        copy.mode = static_cast<read_mode>(m_box_mode->GetSelection());
        std::string text = pdf_to_text(app->getPdfInfo(), copy);
        Json::Value value = text;
        reader_output->ShowText(value.toStyledString());
    } catch (const xpdf_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
    }
}