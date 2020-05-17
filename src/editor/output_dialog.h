#ifndef __OUTPUT_DIALOG_H__
#define __OUTPUT_DIALOG_H__

#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <json/json.h>

#include "../shared/layout.h"

class output_dialog : public wxDialog {
public:
    output_dialog(wxWindow *parent, const bill_layout_script &layout);

private:
    wxListCtrl *m_list_ctrl;

    void addItems(const Json::Value &json_output);
};

#endif