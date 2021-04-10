#ifndef __LAYOUT_OPTIONS_DIALOG_H__
#define __LAYOUT_OPTIONS_DIALOG_H__

#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/checkbox.h>

#include "layout.h"

class LayoutOptionsDialog : public wxDialog {
public:
    LayoutOptionsDialog(wxWindow *parent, layout_box_list *m_layout);

    void OnOK(wxCommandEvent &evt);

private:
    layout_box_list *m_layout;

    wxListBox *m_lang_list;
    wxCheckBox *m_setlayout_box;

    std::vector<intl::language> lang_codes;

    DECLARE_EVENT_TABLE()
};

#endif