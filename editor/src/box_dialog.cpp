#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

#include "editor.h"
#include "utils.h"

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

box_dialog::box_dialog(frame_editor *parent, layout_box &box) :
    wxDialog(parent, wxID_ANY, "Modifica Rettangolo", wxDefaultPosition, wxSize(700, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), box(box), app(parent)
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
    addLabelAndCtrl("Nome:", 0, m_box_name);

    m_box_type = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    for (const char *str : box_type_labels) {
        m_box_type->Append(str);
    }
    m_box_type->SetToolTip("Contenuto");
    m_box_type->SetSelection(int(box.type));

    m_box_mode = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    for (const char *str : read_mode_labels) {
        m_box_mode->Append(str);
    }
    m_box_mode->SetToolTip("Specifica il metodo di lettura");
    m_box_mode->SetSelection(int(box.mode));

    wxButton *testButton = new wxButton(this, BUTTON_TEST, "Leggi contenuto");
    addLabelAndCtrl("Opzioni:", 0, m_box_type, m_box_mode, testButton);

    m_box_spacers = new wxTextCtrl(this, wxID_ANY, box.spacers);
    addLabelAndCtrl("Spaziatori:", 0, m_box_spacers);

    m_box_goto_label = new wxTextCtrl(this, wxID_ANY, box.goto_label);
    addLabelAndCtrl("Label goto:", 0, m_box_goto_label);

    CreateScriptBox();
    m_box_script->SetValue(box.script);
    addLabelAndCtrl("Script:", 1, m_box_script);

    top_level->Add(sizer, 1, wxEXPAND | wxALL, 5);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    top_level->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    okCancelSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_CANCEL, "Annulla"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_APPLY, "Applica"), 0 ,wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_HELP, "Aiuto"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    top_level->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizer(top_level);
    Show();

    reader_output = new TextDialog(this, "Risultato Lettura");
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_APPLY, box_dialog::OnApply)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, box_dialog::OnCancel)
    EVT_BUTTON(wxID_HELP, box_dialog::OnClickHelp)
    EVT_BUTTON(BUTTON_TEST, box_dialog::OnClickTest)
    EVT_CLOSE(box_dialog::OnClose)
END_EVENT_TABLE()

bool box_dialog::saveBox() {
    if (Validate()) {
        box.name = m_box_name->GetValue();
        box.type = static_cast<box_type>(m_box_type->GetSelection());
        box.mode = static_cast<read_mode>(m_box_mode->GetSelection());
        box.spacers = m_box_spacers->GetValue();
        box.goto_label = m_box_goto_label->GetValue();
        box.script = m_box_script->GetValue();
        app->updateLayout();
        return true;
    }
    return false;
}

void box_dialog::CreateScriptBox() {
    wxStyledTextCtrl *text = new wxStyledTextCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    text->StyleSetFont(0, font);
    text->SetEOLMode(wxSTC_EOL_LF);
    m_box_script = text;
}

void box_dialog::OnApply(wxCommandEvent &evt) {
    saveBox();
}

void box_dialog::OnOK(wxCommandEvent &evt) {
    if (saveBox()) {
        Close();
    }
}

void box_dialog::OnCancel(wxCommandEvent &evt) {
    Close();
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
    pdf_rect copy(dynamic_cast<const pdf_rect &>(box));
    copy.type = static_cast<box_type>(m_box_type->GetSelection());
    copy.mode = static_cast<read_mode>(m_box_mode->GetSelection());
    std::string text = app->getPdfDocument().get_text(copy);
    reader_output->ShowText(wxString(text.c_str(), wxConvUTF8));
}