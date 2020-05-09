#ifndef __BOX_DIALOG_H__
#define __BOX_DIALOG_H__

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/choice.h>

#include "../shared/layout.h"

class box_dialog : public wxDialog {
public:
    box_dialog(wxWindow *parent, layout_box &box);

private:
    bool validateData();

    void OnOK(wxCommandEvent &evt);
    void OnClickHelp(wxCommandEvent &evt);

private:
    layout_box &box;

    wxTextCtrl *m_box_name;
    wxTextCtrl *m_box_parser;
    wxChoice *m_box_type;

    DECLARE_EVENT_TABLE()
};


#endif