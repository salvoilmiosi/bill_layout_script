#include "output_dialog.h"

#include <sstream>

#include <wx/statline.h>

#include "resources.h"
#include "editor.h"

enum {
    CTL_OUTPUT_PAGE,
    TOOL_UPDATE,
    TOOL_ABORT,
};

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);
BEGIN_EVENT_TABLE(output_dialog, wxDialog)
    EVT_MENU(TOOL_UPDATE, output_dialog::OnClickUpdate)
    EVT_MENU(TOOL_ABORT, output_dialog::OnClickAbort)
    EVT_COMBOBOX (CTL_OUTPUT_PAGE, output_dialog::OnPageSelect)
    EVT_TEXT_ENTER (CTL_OUTPUT_PAGE, output_dialog::OnPageSelect)
    EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_READ_COMPLETE, output_dialog::OnReadCompleted)
END_EVENT_TABLE()

DECLARE_RESOURCE(tool_reload_png)
DECLARE_RESOURCE(tool_abort_png)

output_dialog::output_dialog(frame_editor *parent) :
    wxDialog(parent, wxID_ANY, "Lettura dati", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    parent(parent)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxToolBar *toolbar = new wxToolBar(this, wxID_ANY);

    auto loadPNG = [](const auto &resource) {
        return wxBitmap::NewFromPNGData(resource.data, resource.len);
    };

    toolbar->AddTool(TOOL_UPDATE, "Aggiorna", loadPNG(GET_RESOURCE(tool_reload_png)));
    toolbar->AddTool(TOOL_ABORT, "Stop", loadPNG(GET_RESOURCE(tool_abort_png)));

    toolbar->AddStretchableSpace();

    m_page = new wxComboBox(toolbar, CTL_OUTPUT_PAGE, wxEmptyString, wxDefaultPosition, wxDefaultSize);

    toolbar->AddControl(m_page, "Pagina");

    toolbar->Realize();
    sizer->Add(toolbar, 0, wxEXPAND);

    m_list_ctrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(320, 450), wxLC_REPORT);

    sizer->Add(m_list_ctrl, 1, wxEXPAND | wxALL, 5);

    wxButton *ok = new wxButton(this, wxID_OK, "Chiudi");
    sizer->Add(ok, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

    SetSizerAndFit(sizer);
}

void output_dialog::OnClickUpdate(wxCommandEvent &) {
    compileAndRead();
}

void output_dialog::OnClickAbort(wxCommandEvent &) {
    wxCriticalSectionLocker lock(m_thread_cs);
    if (m_thread) {
        m_thread->abort();
    } else {
        wxBell();
    }
}

reader_thread::~reader_thread() {
    wxCriticalSectionLocker lock(parent->m_thread_cs);
    parent->m_thread = nullptr;
}

wxThread::ExitCode reader_thread::Entry() {
    wxString cmd_str = get_app_path() + "layout_compiler";

    const char *args1[] = {
        cmd_str.c_str(),
        "-q", "-", "-o", "temp.out",
        nullptr
    };

    process = open_process(args1);
    std::ostringstream oss;
    oss << layout;
    process->write_all(oss.str());
    process->close_stdin();
    std::string compile_output = process->read_all();
    process.reset();

    if (!compile_output.empty()) {
        wxMessageBox("Errore nella compilazione:\n" + compile_output, "Errore", wxICON_ERROR);
        return (wxThread::ExitCode) 1;
    }

    cmd_str = get_app_path() + "layout_reader";

    const char *args2[] = {
        cmd_str.c_str(),
        "-p", pdf_filename.c_str(),
        "temp.out",
        nullptr
    };

    process = open_process(args2);
    std::istringstream iss(process->read_all());
    process.reset();

    if (wxFileExists("temp.out")) {
        wxRemoveFile("temp.out");

        if (!iss.str().empty()) {
            Json::Value json_output;
            iss >> json_output;

            if (json_output["error"].asBool()) {
                wxMessageBox("Errore in lettura:\n" + json_output["message"].asString(), "Errore", wxICON_ERROR);
                return (wxThread::ExitCode) 1;
            } else {
                wxCriticalSectionLocker lock(parent->m_thread_cs);
                parent->json_values = json_output["values"];

                wxQueueEvent(parent, new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE));
                return (wxThread::ExitCode) 0;
            }
        }
    }
    return (wxThread::ExitCode) 1;
}

void reader_thread::abort() {
    process->abort();
}

void output_dialog::compileAndRead() {
    wxCriticalSectionLocker lock(m_thread_cs);
    if (m_thread || parent->getPdfFilename().empty()) {
        wxBell();
    } else {
        m_thread = new reader_thread(this, parent->layout, parent->getPdfFilename());
        if (m_thread->Run() != wxTHREAD_NO_ERROR) {
            delete m_thread;
            m_thread = nullptr;
        }

        m_page->Clear();
        m_list_ctrl->ClearAll();
    }
}

void output_dialog::OnReadCompleted(wxCommandEvent &evt) {
    wxCriticalSectionLocker lock(m_thread_cs);
    for (int i=1; i <= (int)json_values.size(); ++i) {
        m_page->Append(wxString::Format("%i", i));
    }
    updateItems(0);
}

void output_dialog::OnPageSelect(wxCommandEvent &evt) {
    long num;
    if (m_page->GetValue().ToLong(&num)) {
        updateItems(num - 1);
    } else {
        wxBell();
    }
}

void output_dialog::updateItems(int selected_page) {
    wxCriticalSectionLocker lock(m_thread_cs);

    if (selected_page >= (int)json_values.size() || selected_page < 0) {
        wxBell();
        return;
    }

    m_page->SetSelection(selected_page);

    auto &json_object = json_values[selected_page];

    m_list_ctrl->ClearAll();

    auto col_name = m_list_ctrl->AppendColumn("Nome", wxLIST_FORMAT_LEFT, 150);
    auto col_value = m_list_ctrl->AppendColumn("Valore", wxLIST_FORMAT_LEFT, 150);

    size_t n=0;
    for (Json::ValueConstIterator it = json_object.begin(); it != json_object.end(); ++it) {
        for (size_t i=0; i < it->size(); ++i) {
            wxListItem item;
            item.SetId(n);
            m_list_ctrl->InsertItem(item);

            if (i == 0) {
                m_list_ctrl->SetItem(n, col_name, it.name());
            }
            m_list_ctrl->SetItem(n, col_value, wxString((*it)[Json::Value::ArrayIndex(i)].asCString(), wxConvUTF8));
            ++n;
        }
    }
}