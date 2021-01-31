#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/radiobut.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "editor.h"
#include "utils.h"

#define BOX(x, y) y
constexpr const char *box_type_labels[] = BOX_TYPES;
#undef BOX
#define MODE(x, y, z) y
static const char *read_mode_labels[] = READ_MODES;
#undef MODE

template<typename T>
class RadioGroupValidator : public wxValidator {
protected:
    T m_radio;
    T *m_value;

public:
    RadioGroupValidator(int n, T &m) : m_radio(static_cast<T>(n)), m_value(&m) {}
    
    virtual wxObject *Clone() const override {
        return new RadioGroupValidator(*this);
    }

    virtual bool TransferFromWindow() override {
        if (m_value) {
            wxRadioButton *radio_btn = dynamic_cast<wxRadioButton*>(GetWindow());
            if (radio_btn->GetValue()) {
                *m_value = m_radio;
            }
        }
        return true;
    }

    virtual bool TransferToWindow() override {
        if (m_value) {
            wxRadioButton *radio_btn = dynamic_cast<wxRadioButton*>(GetWindow());
            if (*m_value == m_radio) {
                radio_btn->SetValue(true);
            }
        }
        return true;
    }

    virtual bool Validate(wxWindow *) override {
        return true;
    }
};

box_dialog::box_dialog(frame_editor *parent, layout_box &box) :
    wxDialog(parent, wxID_ANY, "Modifica Rettangolo", wxDefaultPosition, wxSize(700, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), box(box), app(parent)
{
    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    auto addLabelAndCtrl = [&](const wxString &labelText, int vprop, int hprop, auto* ... ctrls) {
        wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText *label = new wxStaticText(this, wxID_ANY, labelText, wxDefaultPosition, wxSize(60, -1), wxALIGN_RIGHT);
        hsizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

        (hsizer->Add(ctrls, hprop, wxEXPAND | wxLEFT, 5), ...);

        sizer->Add(hsizer, vprop, wxEXPAND | wxALL, 5);
    };
    
    m_box_name = new wxTextCtrl(this, wxID_ANY, box.name);
    addLabelAndCtrl("Nome:", 0, 1, m_box_name);

    auto add_radio_btns = [&] <size_t N> (const wxString &label, const char *const (& btn_labels)[N], auto &value) {
        bool first = true;
        auto create_btn = [&](size_t i) {
            auto ret = new wxRadioButton(this, wxID_ANY, btn_labels[i],
                wxDefaultPosition, wxDefaultSize, first ? wxRB_GROUP : 0,
                RadioGroupValidator(i, value));
            first = false;
            return ret;
        };
        [&] <size_t ... Is> (std::index_sequence<Is...>) {
            addLabelAndCtrl(label, 0, 0, create_btn(Is) ...);
        }(std::make_index_sequence<N>{});
    };

    add_radio_btns("Tipo:", box_type_labels, box.type);
    add_radio_btns(L"Modalità:", read_mode_labels, box.mode);

    auto make_script_box = [&](const wxString &value) {
        wxStyledTextCtrl *text = new wxStyledTextCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
        wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        text->StyleSetFont(0, font);
        text->SetEOLMode(wxSTC_EOL_LF);
        text->SetValue(value);
        return text;
    };

    m_box_goto_label = new wxTextCtrl(this, wxID_ANY, box.goto_label);
    addLabelAndCtrl("Label goto:", 0, 1, m_box_goto_label);

    m_box_spacers = make_script_box(box.spacers);
    addLabelAndCtrl("Spaziatori:", 1, 1, m_box_spacers);

    m_box_script = make_script_box(box.script);
    addLabelAndCtrl("Script:", 3, 1, m_box_script);

    top_level->Add(sizer, 1, wxEXPAND | wxALL, 5);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    top_level->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    okCancelSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_CANCEL, "Annulla"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_APPLY, "Applica"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    top_level->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizer(top_level);
    Show();

    reader_output = new TextDialog(this, "Risultato Lettura");
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_APPLY, box_dialog::OnApply)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, box_dialog::OnCancel)
    EVT_CLOSE(box_dialog::OnClose)
END_EVENT_TABLE()

bool box_dialog::saveBox() {
    if (Validate()) {
        TransferDataFromWindow();
        box.name = m_box_name->GetValue();
        box.spacers = m_box_spacers->GetValue();
        box.goto_label = m_box_goto_label->GetValue();
        box.script = m_box_script->GetValue();
        app->updateLayout();
        return true;
    }
    return false;
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