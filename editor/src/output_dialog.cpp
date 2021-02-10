#include "output_dialog.h"

#include <wx/statline.h>
#include <wx/filename.h>

#include "resources.h"
#include "editor.h"

#include "reader.h"
#include "utils.h"

enum {
    CTL_DEBUG,
    CTL_OUTPUT_PAGE,
    TOOL_UPDATE,
    TOOL_ABORT,
};

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_COMMAND_LAYOUT_ERROR, wxThreadEvent);
BEGIN_EVENT_TABLE(output_dialog, wxDialog)
    EVT_MENU(TOOL_UPDATE, output_dialog::OnClickUpdate)
    EVT_MENU(TOOL_ABORT, output_dialog::OnClickAbort)
    EVT_CHECKBOX(CTL_DEBUG, output_dialog::OnUpdate)
    EVT_COMBOBOX (CTL_OUTPUT_PAGE, output_dialog::OnUpdate)
    EVT_TEXT_ENTER (CTL_OUTPUT_PAGE, output_dialog::OnUpdate)
    EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_READ_COMPLETE, output_dialog::OnReadCompleted)
    EVT_COMMAND(wxID_ANY, wxEVT_COMMAND_LAYOUT_ERROR, output_dialog::OnLayoutError)
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

    error_dialog = new TextDialog(this, "Errore di Layout");
}

void output_dialog::OnClickUpdate(wxCommandEvent &) {
    compileAndRead();
}

void output_dialog::OnClickAbort(wxCommandEvent &) {
    if (m_thread) {
        m_thread->abort();
    } else {
        wxBell();
    }
}

reader_thread::~reader_thread() {
    parent->m_thread = nullptr;
}

wxThread::ExitCode reader_thread::Entry() {
    try {
        bill_layout_script layout_copy = layout;
        if (layout_copy.filename().empty()) {
            layout_copy.set_filename(parent->parent->getControlScript().ToStdString());
        }
        m_reader.add_layout(std::move(layout_copy));
        m_reader.start();
        if (!m_aborted) {
            parent->m_output = m_reader.get_output();
            wxQueueEvent(parent, new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE));

            if (!parent->m_output.warnings.empty()) {
                auto *evt = new wxThreadEvent(wxEVT_COMMAND_LAYOUT_ERROR);
                std::string warnings = string_join(parent->m_output.warnings, "\n\n");
                evt->SetString(wxString(warnings.c_str(), wxConvUTF8));
                wxQueueEvent(parent, evt);
            }
            return (wxThread::ExitCode) 0;
        }
    } catch (const std::exception &error) {
        auto *evt = new wxThreadEvent(wxEVT_COMMAND_LAYOUT_ERROR);
        evt->SetString(wxString(error.what(), wxConvUTF8));
        wxQueueEvent(parent, evt);
    }
    return (wxThread::ExitCode) 1;
}

void reader_thread::abort() {
    m_aborted = true;
    m_reader.halt();
}

void output_dialog::compileAndRead() {
    if (m_thread || ! parent->getPdfDocument().isopen()) {
        wxBell();
    } else {
        m_thread = new reader_thread(this, parent->layout, parent->getPdfDocument());
        if (m_thread->Run() != wxTHREAD_NO_ERROR) {
            delete m_thread;
            m_thread = nullptr;
        }

        m_page->Clear();
        m_list_ctrl->ClearAll();
    }
}

void output_dialog::OnLayoutError(wxCommandEvent &evt) {
    error_dialog->ShowText(evt.GetString());
}

void output_dialog::OnReadCompleted(wxCommandEvent &evt) {
    m_page->Append("Globali");
    for (int i=1; i <= m_output.table_index + 1; ++i) {
        m_page->Append(wxString::Format("%i", i));
    }
    m_page->SetSelection(m_output.values.size() > 0 ? 1 : 0);
    updateItems();
}

void output_dialog::OnUpdate(wxCommandEvent &evt) {
    updateItems();
}

void output_dialog::updateItems() {
    m_list_ctrl->ClearAll();

    auto col_name = m_list_ctrl->AppendColumn("Nome", wxLIST_FORMAT_LEFT, 150);
    auto col_value = m_list_ctrl->AppendColumn("Valore", wxLIST_FORMAT_LEFT, 150);

    auto display_page = [&](const variable_map &map, int table_index) {
        size_t n=0;
        std::string old_name;
        for (auto &[key, var] : map) {
            if (key.table_index != table_index) continue;

            if (!m_show_debug->GetValue() && key.name.front() == '_') {
                continue;
            }
            wxListItem item;
            item.SetId(n);
            m_list_ctrl->InsertItem(item);

            if (old_name != key.name) {
                m_list_ctrl->SetItem(n, col_name, key.name);
            }
            m_list_ctrl->SetItem(n, col_value, wxString(var.str().c_str(), wxConvUTF8));
            old_name = key.name;
            ++n;
        }
    };

    if (m_page->GetValue() == "Globali") {
        m_page->SetSelection(0);
        
        display_page(m_output.values, variable_key::global_index);
    } else {
        long selected_page;
        if (!m_page->GetValue().ToLong(&selected_page)) {
            wxBell();
            return;
        }

        if (selected_page > m_output.values.size() || selected_page <= 0) {
            wxBell();
            return;
        }

        m_page->SetSelection(selected_page);
        --selected_page;

        display_page(m_output.values, selected_page);
    }
}