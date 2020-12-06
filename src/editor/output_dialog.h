#ifndef __OUTPUT_DIALOG_H__
#define __OUTPUT_DIALOG_H__

#include <wx/dialog.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/thread.h>
#include <json/json.h>

#include "editor.h"
#include "subprocess.h"

class output_dialog;

class reader_thread : public wxThread {
public:
    reader_thread(output_dialog *parent, const bill_layout_script &layout, const wxString &pdf_filename)
        : parent(parent), layout(layout), pdf_filename(pdf_filename) {}
    ~reader_thread();

    void abort();

protected:
    virtual ExitCode Entry();

private:
    std::unique_ptr<subprocess> process;

    wxString temp_file;

    output_dialog *parent;

    Json::Value json_values;

    bill_layout_script layout;
    wxString pdf_filename;
};

class output_dialog : public wxDialog {
public:
    output_dialog(frame_editor *parent);
    void compileAndRead();

private:
    frame_editor *parent;

    wxCheckBox *m_show_debug;
    wxComboBox *m_page;
    wxListCtrl *m_list_ctrl;

    reader_thread *m_thread = nullptr;
    Json::Value json_output;
    wxString compile_error;
    
    wxCriticalSection m_thread_cs;

    void OnUpdate(wxCommandEvent &evt);
    void OnClickUpdate(wxCommandEvent &evt);
    void OnClickAbort(wxCommandEvent &evt);

    void OnReadCompleted(wxCommandEvent &evt);
    void OnCompileError(wxCommandEvent &evt);
    void updateItems();

    DECLARE_EVENT_TABLE()

    friend class reader_thread;
};

#endif