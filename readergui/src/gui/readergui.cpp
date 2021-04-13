#include "readergui.h"

#include <wx/sizer.h>

#include "dati_fattura.h"

BEGIN_EVENT_TABLE(ReaderGui, wxFrame)

END_EVENT_TABLE()

ReaderGui::ReaderGui() : wxFrame(nullptr, wxID_ANY, "Reader GUI", wxDefaultPosition, wxSize(900, 700)) {
    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    auto *m_table = new VariableMapTable(this);

    top_level->Add(m_table, 1, wxEXPAND | wxALL, 5);

    SetSizer(top_level);
    Show();
}