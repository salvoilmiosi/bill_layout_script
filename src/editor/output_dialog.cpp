#include "output_dialog.h"

#include <wx/statline.h>
#include <wx/filename.h>

#include "resources.h"
#include "editor.h"
#include "compile_error_diag.h"
#include "arguments.h"
#include "utils.h"

enum {
    CTL_DEBUG,
    CTL_OUTPUT_PAGE,
    TOOL_UPDATE,
    TOOL_ABORT,
};

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_COMPILE_ERROR, wxThreadEvent);
BEGIN_EVENT_TABLE(output_dialog, wxDialog)
    EVT_MENU(TOOL_UPDATE, output_dialog::OnClickUpdate)
    EVT_MENU(TOOL_ABORT, output_dialog::OnClickAbort)
    EVT_CHECKBOX(CTL_DEBUG, output_dialog::OnUpdate)
    EVT_COMBOBOX (CTL_OUTPUT_PAGE, output_dialog::OnUpdate)
    EVT_TEXT_ENTER (CTL_OUTPUT_PAGE, output_dialog::OnUpdate)
    EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_READ_COMPLETE, output_dialog::OnReadCompleted)
    EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_COMPILE_ERROR, output_dialog::OnCompileError)
END_EVENT_TABLE()

DECLARE_RESOURCE(tool_reload_png)
DECLARE_RESOURCE(tool_abort_png)

output_dialog::output_dialog(frame_editor *parent) :
    wxDialog(parent, wxID_ANY, "Lettura Dati", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
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

    m_show_debug = new wxCheckBox(toolbar, CTL_DEBUG, "Mostra debug");
    toolbar->AddControl(m_show_debug, "Mostra debug");

    m_page = new wxComboBox(toolbar, CTL_OUTPUT_PAGE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxTE_PROCESS_ENTER);

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
    wxString cmd_compiler = get_app_path() + "layout_compiler";
    wxString cmd_reader = get_app_path() + "layout_reader";

    subprocess proc_compiler(arguments(
        cmd_compiler,
        "-q", "-o", "-", "-"
    ));
    proc_compiler.stream_in << layout;
    proc_compiler.stream_in.close();

    std::string compile_error = read_all(proc_compiler.stream_err);
    if (!compile_error.empty()) {
        auto *evt = new wxThreadEvent(wxEVT_COMMAND_COMPILE_ERROR);
        evt->SetString(compile_error);
        wxQueueEvent(parent, evt);
        return (wxThread::ExitCode) 1;
    }

    proc_reader = std::make_unique<subprocess>(arguments(
        cmd_reader,
        "-p", pdf_filename,
        "-d", "-"
    ));

    std::copy(
        std::istreambuf_iterator<char>(proc_compiler.stream_out),
        std::istreambuf_iterator<char>(),
        std::ostreambuf_iterator<char>(proc_reader->stream_in)
    );
    proc_reader->stream_in.close();

    try {
        Json::Value json_output;
        proc_reader->stream_out >> json_output;

        if (json_output["error"].asBool()) {
            wxMessageBox("Errore in lettura:\n" + json_output["message"].asString(), "Errore", wxICON_INFORMATION);
            return (wxThread::ExitCode) 1;
        } else {
            parent->json_output = json_output;
            wxQueueEvent(parent, new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE));
            return (wxThread::ExitCode) 0;
        }
    } catch (std::exception &) {
        return (wxThread::ExitCode) 1;
    }
}

void reader_thread::abort() {
    proc_reader->abort();
}

void output_dialog::compileAndRead() {
    wxCriticalSectionLocker lock(m_thread_cs);
    if (m_thread || ! parent->getPdfDocument().isopen()) {
        wxBell();
    } else {
        m_thread = new reader_thread(this, parent->layout, parent->getPdfDocument().filename());
        if (m_thread->Run() != wxTHREAD_NO_ERROR) {
            delete m_thread;
            m_thread = nullptr;
        }

        m_page->Clear();
        m_list_ctrl->ClearAll();
    }
}

void output_dialog::OnCompileError(wxCommandEvent &evt) {
    CompileErrorDialog(this, evt.GetString()).ShowModal();
}

void output_dialog::OnReadCompleted(wxCommandEvent &evt) {
    m_page->Append("Globali");
    for (int i=1; i <= (int)json_output["values"].size(); ++i) {
        m_page->Append(wxString::Format("%i", i));
    }
    m_page->SetSelection(json_output["values"].size() > 0 ? 1 : 0);
    updateItems();
}

void output_dialog::OnUpdate(wxCommandEvent &evt) {
    updateItems();
}

void output_dialog::updateItems() {
    m_list_ctrl->ClearAll();

    auto col_name = m_list_ctrl->AppendColumn("Nome", wxLIST_FORMAT_LEFT, 150);
    auto col_value = m_list_ctrl->AppendColumn("Valore", wxLIST_FORMAT_LEFT, 150);

    if (m_page->GetValue() == "Globali") {
        m_page->SetSelection(0);
        
        auto &json_object = json_output["globals"];

        size_t n=0;
        for (Json::ValueConstIterator it = json_object.begin(); it != json_object.end(); ++it) {
            if (!m_show_debug->GetValue() && it.name().front() == '!') {
                continue;
            }
            wxListItem item;
            item.SetId(n);
            m_list_ctrl->InsertItem(item);

            m_list_ctrl->SetItem(n, col_name, it.name());
            m_list_ctrl->SetItem(n, col_value, wxString(it->asCString(), wxConvUTF8));
            ++n;
        }

    } else {
        long selected_page;
        if (!m_page->GetValue().ToLong(&selected_page)) {
            wxBell();
            return;
        }

        if (selected_page > (int)json_output["values"].size() || selected_page <= 0) {
            wxBell();
            return;
        }

        m_page->SetSelection(selected_page);
        --selected_page;

        auto &json_object = json_output["values"][static_cast<int>(selected_page)];

        size_t n=0;
        for (Json::ValueConstIterator it = json_object.begin(); it != json_object.end(); ++it) {
            if (!m_show_debug->GetValue() && it.name().front() == '!') {
                continue;
            }
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
}