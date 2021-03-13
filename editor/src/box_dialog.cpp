#include "box_dialog.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/radiobut.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/stc/stc.h>

#include "editor.h"
#include "utils.h"
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
};

template<typename WindowType = wxTextCtrl>
class StringValidator : public wxValidator {
protected:
    std::string *m_value;

public:
    StringValidator(std::string *value) : m_value(value) {}

    virtual wxObject *Clone() const override {
        return new StringValidator(*this);
    }

    virtual bool TransferFromWindow() override {
        if (m_value) {
            WindowType *ctl = dynamic_cast<WindowType*>(GetWindow());
            *m_value = ctl->GetValue().ToStdString();
        }
        return true;
    }

    virtual bool TransferToWindow() override {
        if (m_value) {
            WindowType *ctl = dynamic_cast<WindowType*>(GetWindow());
            ctl->SetValue(wxString(m_value->c_str(), wxConvUTF8));
        }
        return true;
    }
};

enum {
    BUTTON_TEST = 1001
};

box_dialog::box_dialog(frame_editor *parent, layout_box &out_box) :
    wxDialog(parent, wxID_ANY, "Modifica Rettangolo", wxDefaultPosition, wxSize(700, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), m_box(out_box), app(parent)
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
    
    addLabelAndCtrl("Nome:", 0, 1, new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, StringValidator(&m_box.name)));

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

    add_radio_btns("Tipo:", box_type_labels, m_box.type);
    add_radio_btns(L"Modalità:", read_mode_labels, m_box.mode);

    auto make_script_box = [&](std::string &value) {
        wxStyledTextCtrl *text = new wxStyledTextCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
        wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        text->StyleSetFont(0, font);
        text->SetEOLMode(wxSTC_EOL_LF);
        text->SetValidator(StringValidator<wxStyledTextCtrl>(&value));
        return text;
    };

    addLabelAndCtrl("Label goto:", 0, 1, new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, StringValidator(&m_box.goto_label)));
    addLabelAndCtrl("Spaziatori:", 1, 1, make_script_box(m_box.spacers));
    addLabelAndCtrl("Script:", 3, 1, make_script_box(m_box.script));

    top_level->Add(sizer, 1, wxEXPAND | wxALL, 5);

    wxStaticLine *line = new wxStaticLine(this, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    top_level->Add(line, 0, wxGROW | wxALL, 5);

    wxBoxSizer *okCancelSizer = new wxBoxSizer(wxHORIZONTAL);

    okCancelSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_CANCEL, "Annulla"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, wxID_APPLY, "Applica"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    okCancelSizer->Add(new wxButton(this, BUTTON_TEST, "Test"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    top_level->Add(okCancelSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizer(top_level);
    Show();

    reader_output = new TextDialog(this, "Risultato Lettura");
}

std::map<layout_box *, box_dialog *> box_dialog::open_dialogs;

box_dialog *box_dialog::openDialog(frame_editor *parent, layout_box &out_box) {
    auto it = open_dialogs.find(&out_box);
    if (it != open_dialogs.end()) {
        it->second->Raise();
        return it->second;
    } else {
        return open_dialogs[&out_box] = new box_dialog(parent, out_box);
    }
}

bool box_dialog::closeDialog(layout_box &out_box) {
    auto it = open_dialogs.find(&out_box);
    if (it != open_dialogs.end()) {
        it->second->Raise();
        return it->second->Close();
    }
    return true;
}

bool box_dialog::closeAll() {
    bool ret = true;
    auto open_dialogs_copy = open_dialogs;
    for (auto &[box, dialog] : open_dialogs_copy) {
        if (!dialog->Close()) ret = false;
    }
    return ret;
}

BEGIN_EVENT_TABLE(box_dialog, wxDialog)
    EVT_BUTTON(wxID_APPLY, box_dialog::OnApply)
    EVT_BUTTON(wxID_OK, box_dialog::OnOK)
    EVT_BUTTON(wxID_CANCEL, box_dialog::OnCancel)
    EVT_BUTTON(BUTTON_TEST, box_dialog::OnTest)
    EVT_CLOSE(box_dialog::OnClose)
END_EVENT_TABLE()

void box_dialog::saveBox() {
    TransferDataFromWindow();
    app->updateLayout();
    app->selectBox(&m_box);
}

void box_dialog::OnApply(wxCommandEvent &evt) {
    saveBox();
}

void box_dialog::OnOK(wxCommandEvent &evt) {
    saveBox();
    Close();
}

void box_dialog::OnCancel(wxCommandEvent &evt) {
    Close();
}

void box_dialog::OnTest(wxCommandEvent &evt) {
    auto box_copy = m_box;
    TransferDataFromWindow();
    std::string text = app->getPdfDocument().get_text(m_box, app->getBoxRotation());
    m_box = box_copy;
    reader_output->ShowText(wxString(text.c_str(), wxConvUTF8));
}

static bool operator == (const layout_box &a, const layout_box &b) {
    return a.name == b.name
        && a.script == b.script
        && a.spacers == b.spacers
        && a.goto_label == b.goto_label
        && a.mode == b.mode
        && a.type == b.type;
}

void box_dialog::OnClose(wxCloseEvent &evt) {
    auto box_copy = m_box;
    TransferDataFromWindow();
    if (box_copy != m_box) {
        wxMessageDialog dialog(this, "Salvare le modifiche?", "Layout Bolletta", wxYES_NO | wxCANCEL | wxICON_WARNING);

        switch (dialog.ShowModal()) {
        case wxID_YES:
            app->updateLayout();
            app->selectBox(&m_box);
            break;
        case wxID_NO:
            m_box = box_copy;
            break;
        case wxID_CANCEL:
            m_box = box_copy;
            evt.Veto();
            return;
        }
    }
    open_dialogs.erase(&m_box);
    Destroy();
}