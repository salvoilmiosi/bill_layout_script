#ifndef __BOX_DIALOG_H__
#define __BOX_DIALOG_H__

#include <wx/dialog.h>
#include <wx/textctrl.h>

#include "../shared/layout.h"

class box_dialog : public wxDialog {
public:
    box_dialog(wxWindow *parent, layout_box &box);

private:
    bool validateAndUpdateBox();

    void OnOK(wxCommandEvent &evt);

private:
    layout_box &box;

    wxTextCtrl *m_box_name;

    DECLARE_EVENT_TABLE()
};


#endif