#ifndef __OUTPUT_DIALOG_H__
#define __OUTPUT_DIALOG_H__

#include <wx/dialog.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <json/json.h>

#include "../shared/layout.h"

class output_dialog : public wxDialog {
public:
    output_dialog(wxWindow *parent, const bill_layout_script &layout, const std::string &pdf_filename);

private:
    wxComboBox *m_page;
    wxListCtrl *m_list_ctrl;

    Json::Value json_values;

    void OnPageSelect(wxCommandEvent &evt);
    void updateItems(int selected_page);

    DECLARE_EVENT_TABLE()
};

#endif