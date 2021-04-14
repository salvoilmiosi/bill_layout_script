#include "readergui.h"

#include <filesystem>

#include <wx/sizer.h>
#include <wx/thread.h>

#include "reader.h"
#include "layout.h"

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);

enum {
    MENU_OPEN_FOLDER = 10000,
    MENU_SET_SCRIPT
};

BEGIN_EVENT_TABLE(ReaderGui, wxFrame)
    EVT_MENU(MENU_OPEN_FOLDER, ReaderGui::OnOpenFolder)
    EVT_MENU(MENU_SET_SCRIPT, ReaderGui::OnSetScript)
END_EVENT_TABLE()

class ReaderThread : public wxThread {
public:
    ReaderThread(ReaderGui *parent) : wxThread(wxTHREAD_JOINABLE), parent(parent) {}

    void recompile() {
        my_reader.halt();
        my_reader.clear();
        my_reader.add_flags(reader_flags::RECURSIVE);
        my_reader.add_layout(parent->getControlScript());
    }

protected:
    ReaderGui *parent;

    reader my_reader;
    
    virtual ExitCode Entry() override {
        recompile();

        while (parent->m_running) {
            pdf_document doc;
            try {
                doc.open(parent->m_queue.dequeue());
                my_reader.set_document(doc);
                my_reader.start();

                auto evt = new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE);
                evt->SetPayload(reader_output{
                    my_reader.get_values(),
                    doc.filename(),
                    my_reader.get_warnings()
                });
                wxQueueEvent(parent, evt);
            } catch (const layout_error &error) {
                auto evt = new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE);
                evt->SetPayload(reader_output{
                    {},
                    doc.filename(),
                    {error.what()}
                });
                wxQueueEvent(parent, evt);
            } catch (...) {}
        }

        return 0;
    }
};

ReaderGui::ReaderGui() : wxFrame(nullptr, wxID_ANY, "Reader GUI", wxDefaultPosition, wxSize(900, 700)) {
    m_config = new wxConfig("BillLayoutScript");

    wxMenuBar *menu_bar = new wxMenuBar;

    wxMenu *menu_reader = new wxMenu;
    menu_reader->Append(MENU_OPEN_FOLDER, "Apri Cartella");
    menu_reader->Append(MENU_SET_SCRIPT, "Imposta Script Layout");

    menu_bar->Append(menu_reader, "Reader");
    SetMenuBar(menu_bar);

    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    m_table = new VariableMapTable(this);
    top_level->Add(m_table, 1, wxEXPAND | wxALL, 5);

    SetSizer(top_level);
    Show();

    Connect(wxEVT_COMMAND_READ_COMPLETE, wxThreadEventHandler(ReaderGui::OnReadCompleted));
    startReaderThreads();
}

ReaderGui::~ReaderGui() {
    m_running = false;
    for (auto &thread : m_threads) {
        thread->Wait();
        delete thread;
    }
}

void ReaderGui::OnReadCompleted(wxThreadEvent &evt) {
    m_table->AddReaderValues(evt.GetPayload<reader_output>());
    m_table->Refresh();
}

void ReaderGui::OnOpenFolder(wxCommandEvent &evt) {
    wxDirDialog diag(this, "Scegli una cartella...");
    if (diag.ShowModal() == wxID_OK) {
        m_table->DeleteAllItems();
        m_queue.clear();
        std::filesystem::recursive_directory_iterator it(diag.GetPath().ToStdString());
        for (const auto &path : it) {
            const auto &filename = path.path();
            if (std::filesystem::is_regular_file(filename) && filename.extension() == ".pdf") {
                m_queue.enqueue(filename);
            }
        }
    }
}

void ReaderGui::OnSetScript(wxCommandEvent &evt) {
    getControlScript(true);
    for (auto &thread : m_threads) {
        dynamic_cast<ReaderThread*>(thread)->recompile();
    }
}

void ReaderGui::startReaderThreads() {
    m_running = true;
    for (auto &thread : m_threads) {
        thread = new ReaderThread(this);
        thread->Run();
    }
}

std::filesystem::path ReaderGui::getControlScript(bool open_dialog) {
    wxString filename = m_config->Read("ControlScriptFilename");
    if (filename.empty() || open_dialog) {
        wxFileDialog diag(this, "Apri script di controllo", filename, wxEmptyString, "File bls (*.bls)|*.bls|Tutti i file (*.*)|*.*");

        if (diag.ShowModal() == wxID_OK) {
            filename = diag.GetPath();
            m_config->Write("ControlScriptFilename", filename);
        }
    }
    return filename.ToStdString();
}