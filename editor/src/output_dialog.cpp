#include "output_dialog.h"

#include <wx/statline.h>
#include <wx/filename.h>

#include "resources.h"
#include "editor.h"

#include "parser.h"
#include "reader.h"
#include "utils.h"

enum {
    CTL_DEBUG,
    CTL_GLOBALS,
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
    EVT_CHECKBOX(CTL_GLOBALS, output_dialog::OnUpdate)
    EVT_SPINCTRL (CTL_OUTPUT_PAGE, output_dialog::OnUpdateSpin)
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

    m_show_debug = new wxCheckBox(toolbar, CTL_DEBUG, "Debug");
    toolbar->AddControl(m_show_debug, "Debug");

    m_show_globals = new wxCheckBox(toolbar, CTL_GLOBALS, "Globali");
    toolbar->AddControl(m_show_globals, "Globali");

    m_page = new wxSpinCtrl(toolbar, CTL_OUTPUT_PAGE, wxEmptyString, wxDefaultPosition, wxSize(150, -1), wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 0);

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

reader_thread::reader_thread(output_dialog *parent, reader &m_reader, const layout_box_list &layout) : parent(parent), m_reader(m_reader) {
    std::ranges::copy(layout, std::back_inserter(m_layout));
    m_layout.language_code = layout.language_code;
}

reader_thread::~reader_thread() {
    parent->m_thread = nullptr;
}

wxThread::ExitCode reader_thread::Entry() {
    try {
        parser my_parser;
        std::filesystem::path path = parent->parent->m_filename;
        if (path.empty()) path = std::filesystem::path(parent->parent->getControlScript().ToStdString());
        my_parser.read_layout(path, m_layout);
        m_reader.add_code(std::move(my_parser).get_bytecode());
        m_reader.start();
        wxQueueEvent(parent, new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE));

        if (!m_reader.get_warnings().empty()) {
            auto *evt = new wxThreadEvent(wxEVT_COMMAND_LAYOUT_ERROR);
            std::string warnings = string_join(m_reader.get_warnings(), "\n\n");
            evt->SetString(wxString::FromUTF8(warnings.c_str()));
            wxQueueEvent(parent, evt);
        }
        return (wxThread::ExitCode) 0;
    } catch (const std::exception &error) {
        auto *evt = new wxThreadEvent(wxEVT_COMMAND_LAYOUT_ERROR);
        evt->SetString(wxString::FromUTF8(error.what()));
        wxQueueEvent(parent, evt);
    } catch (reader_aborted) {
        // ignore output
    }
    return (wxThread::ExitCode) 1;
}

void reader_thread::abort() {
    m_reader.abort();
}

void output_dialog::compileAndRead() {
    if (m_thread || ! parent->getPdfDocument().isopen()) {
        wxBell();
    } else {
        m_reader.clear();
        m_reader.set_document(parent->getPdfDocument());
        m_thread = new reader_thread(this, m_reader, parent->layout);
        if (m_thread->Run() != wxTHREAD_NO_ERROR) {
            delete m_thread;
            m_thread = nullptr;
        }

        m_page->SetRange(0, 0);
        m_page->SetValue("");
        m_list_ctrl->ClearAll();
    }
}

void output_dialog::OnLayoutError(wxCommandEvent &evt) {
    error_dialog->ShowText(evt.GetString());
}

void output_dialog::OnReadCompleted(wxCommandEvent &evt) {
    m_page->SetRange(1, m_reader.get_table_count());
    m_page->SetValue(1);
    updateItems();
}

void output_dialog::OnUpdateSpin(wxSpinEvent &evt) {
    updateItems();
}

void output_dialog::OnUpdate(wxCommandEvent &evt) {
    updateItems();
}

void output_dialog::updateItems() {
    if (m_reader.is_running()) return;

    m_list_ctrl->ClearAll();

    auto col_name = m_list_ctrl->AppendColumn("Nome", wxLIST_FORMAT_LEFT, 150);
    auto col_value = m_list_ctrl->AppendColumn("Valore", wxLIST_FORMAT_LEFT, 150);

    auto display_page = [&](int table_index) {
        size_t n=0;
        std::string old_name;
        for (auto &[key, var] : m_reader.get_values()) {
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
            m_list_ctrl->SetItem(n, col_value, wxString::FromUTF8(var.as_string().c_str()));
            old_name = key.name;
            ++n;
        }
    };

    if (m_show_globals->GetValue()) {
        display_page(variable_key::global_index);
    } else {
        display_page(m_page->GetValue() - 1);
        m_page->SetValue(wxString::Format("%i/%i", m_page->GetValue(), m_page->GetMax()));
    }
}