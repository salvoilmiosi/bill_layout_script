#ifndef __BOX_DIALOG_H__
#define __BOX_DIALOG_H__

#include <wx/dialog.h>

#include "text_dialog.h"
#include "layout.h"

class box_dialog : public wxDialog {
public:
    static box_dialog *openDialog(class frame_editor *app, const box_ptr &box);
    static bool closeDialog(const box_ptr &box);
    static bool closeAll();

private:
    box_dialog(class frame_editor *parent, const box_ptr &box);

private:
    void saveBox();

    void OnApply(wxCommandEvent &evt);
    void OnOK(wxCommandEvent &evt);
    void OnCancel(wxCommandEvent &evt);
    void OnTest(wxCommandEvent &evt);
    void OnClose(wxCloseEvent &evt);

private:
    static std::map<box_ptr, box_dialog *> open_dialogs;

    layout_box m_box;
    box_ptr out_box;

    class frame_editor *app;
    TextDialog *reader_output;

    DECLARE_EVENT_TABLE()
};


#endif