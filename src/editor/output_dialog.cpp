#include "output_dialog.h"

#include <sstream>

#include <wx/statline.h>

#include "../shared/pipe.h"

#include "editor.h"

output_dialog::output_dialog(wxWindow *parent, const bill_layout_script &layout, const std::string &pdf_filename) :
    wxDialog(parent, wxID_ANY, "Lettura dati", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/layout_reader", get_app_path().c_str());

    const char *args[] = {
        cmd_str,
        "-p", pdf_filename.c_str(), "-",
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
    } catch(const std::exception &error) {
        throw pipe_error("Impossibile leggere l'output");
    }

    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(500, 100), wxLC_REPORT);

    addItems(json_output);

    sizer->Add(m_list_ctrl, 1, wxEXPAND | wxALL, 5);

    wxButton *ok = new wxButton(this, wxID_OK, "OK");
    sizer->Add(ok, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizerAndFit(sizer);
}

void output_dialog::addItems(const Json::Value &output) {
    auto &json_values = output["values"][0];
    
    size_t max_lines = 0;

    for (Json::ValueConstIterator it = json_values.begin(); it != json_values.end(); ++it) {
        wxListItem col;
        col.SetId(it - json_values.begin());
        col.SetText(it.name());
        m_list_ctrl->InsertColumn(col.GetId(), col);

        if (it->size() > max_lines) max_lines = it->size();
    }

    for (size_t index = 0; index < max_lines; ++index) {
        wxListItem item;
        item.SetId(index);
        m_list_ctrl->InsertItem(item);
        size_t col = 0;
        for (auto &value : json_values) {
            m_list_ctrl->SetItem(index, col, value[Json::Value::ArrayIndex(index)].asString());
            ++col;
        }
    }
}