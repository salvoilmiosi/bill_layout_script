#ifndef __OUTPUT_DIALOG_H__
#define __OUTPUT_DIALOG_H__

#include <wx/dialog.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/thread.h>

#include "editor.h"
#include "reader.h"
#include "text_dialog.h"

class output_dialog;

class reader_thread : public wxThread {
public:
    reader_thread(output_dialog *parent, reader &m_reader, const layout_box_list &layout);
    ~reader_thread();

    void abort();

protected:
    virtual ExitCode Entry();

private:
    reader &m_reader;
    output_dialog *parent;

    layout_box_list m_layout;
};

class output_dialog : public wxDialog {
public:
    output_dialog(frame_editor *parent);
    void compileAndRead();

private:
    frame_editor *parent;

    wxCheckBox *m_show_globals;
    wxCheckBox *m_show_debug;
    wxSpinCtrl *m_page;
    wxListCtrl *m_list_ctrl;

    reader_thread *m_thread = nullptr;
    reader m_reader;
    
    TextDialog *error_dialog;

    void OnUpdateSpin(wxSpinEvent &evt);
    void OnUpdate(wxCommandEvent &evt);

    void OnClickUpdate(wxCommandEvent &evt);
    void OnClickAbort(wxCommandEvent &evt);

    void OnReadCompleted(wxCommandEvent &evt);
    void OnLayoutError(wxCommandEvent &evt);

    void updateItems();

    DECLARE_EVENT_TABLE()

    friend class reader_thread;
};

#endif