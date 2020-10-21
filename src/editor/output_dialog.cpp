#include "output_dialog.h"

#include <sstream>

#include <wx/statline.h>

#include "../shared/pipe.h"

#include "editor.h"

enum {
    CTL_OUTPUT_PAGE
};

BEGIN_EVENT_TABLE(output_dialog, wxDialog)
    EVT_COMBOBOX (CTL_OUTPUT_PAGE, output_dialog::OnPageSelect)
    EVT_TEXT_ENTER (CTL_OUTPUT_PAGE, output_dialog::OnPageSelect)
END_EVENT_TABLE()

output_dialog::output_dialog(wxWindow *parent, const bill_layout_script &layout, const std::string &pdf_filename) :
    wxDialog(parent, wxID_ANY, "Lettura dati", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    std::string cmd_str = get_app_path() + "/layout_reader";

    const char *args[] = {
        cmd_str.c_str(),
        "-d", "-p", pdf_filename.c_str(), "-",
        nullptr
    };

    auto pipe = open_process(args);
    std::ostringstream oss;
    oss << layout;
    pipe->write_all(oss.str());
    pipe->close_stdin();
    std::istringstream iss(pipe->read_all());
    Json::Value json_output;
    try {
        iss >> json_output;
        json_values = json_output["values"];
    } catch(const std::exception &error) {
        throw pipe_error("Impossibile leggere l'output: " + iss.str());
    }

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    m_page = new wxComboBox(this, CTL_OUTPUT_PAGE, "Pagina", wxDefaultPosition, wxDefaultSize);
    for (int i=1; i <= (int)json_values.size(); ++i) {
        m_page->Append(wxString::Format("%i", i));
    }

    sizer->Add(m_page, 0, wxEXPAND | wxALL, 5);

    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(500, 100), wxLC_REPORT);

    updateItems(0);

    sizer->Add(m_list_ctrl, 1, wxEXPAND | wxALL, 5);

    wxButton *ok = new wxButton(this, wxID_OK, "OK");
    sizer->Add(ok, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizerAndFit(sizer);
}


void output_dialog::OnPageSelect(wxCommandEvent &evt) {
    try {
        updateItems(std::stoi(m_page->GetValue().ToStdString()) - 1);
    } catch (const std::invalid_argument &error) {
        wxBell();
    }
}

void output_dialog::updateItems(int selected_page) {
    if (selected_page >= (int)json_values.size() || selected_page < 0) {
        wxBell();
        return;
    }

    m_page->SetSelection(selected_page);

    auto &json_object = json_values[selected_page];
    
    size_t max_lines = 0;

    m_list_ctrl->ClearAll();

    for (Json::ValueConstIterator it = json_object.begin(); it != json_object.end(); ++it) {
        wxListItem col;
        col.SetId(it - json_object.begin());
        col.SetText(it.name());
        m_list_ctrl->InsertColumn(col.GetId(), col);

        if (it->size() > max_lines) max_lines = it->size();
    }

    for (size_t index = 0; index < max_lines; ++index) {
        wxListItem item;
        item.SetId(index);
        m_list_ctrl->InsertItem(item);
        size_t col = 0;
        for (auto &value : json_object) {
            m_list_ctrl->SetItem(index, col, value[Json::Value::ArrayIndex(index)].asString());
            ++col;
        }
    }
}