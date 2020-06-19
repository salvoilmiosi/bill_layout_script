#ifndef __BOX_DIALOG_H__
#define __BOX_DIALOG_H__

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/choice.h>

#include "../shared/layout.h"

class box_dialog : public wxDialog {
public:
    box_dialog(class frame_editor *parent, layout_box &box);

private:
    bool validateData();

    void OnOK(wxCommandEvent &evt);
    void OnClickHelp(wxCommandEvent &evt);
    void OnClickTest(wxCommandEvent &evt);

private:
    layout_box &box;
    class frame_editor *app;

    wxTextCtrl *m_box_name;
    wxTextCtrl *m_box_spacers;
    wxTextCtrl *m_box_goto_label;
    wxTextCtrl *m_box_script;
    wxTextCtrl *reader_output;
    wxChoice *m_box_type;
    wxChoice *m_box_mode;

    DECLARE_EVENT_TABLE()
};


#endif