#include "readergui.h"

#include <filesystem>

#include <wx/sizer.h>
#include <wx/thread.h>

#include "reader.h"
#include "layout.h"

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);

enum {
    MENU_OPEN_FOLDER = 10000,
    MENU_SET_SCRIPT,
    MENU_RESTART
};

BEGIN_EVENT_TABLE(ReaderGui, wxFrame)
    EVT_MENU(MENU_OPEN_FOLDER, ReaderGui::OnOpenFolder)
    EVT_MENU(MENU_SET_SCRIPT, ReaderGui::OnSetScript)
    EVT_MENU(MENU_RESTART, ReaderGui::OnRestart)
END_EVENT_TABLE()

class ReaderThread : public wxThread {
public:
    ReaderThread(ReaderGui *parent) : wxThread(wxTHREAD_JOINABLE), parent(parent) {}

    void compile_layout() {
        m_halted = m_reader.is_running();
        m_reader.halt();
        m_reader.clear();
        m_reader.add_flags(reader_flags::RECURSIVE);
        m_reader.add_layout(parent->getControlScript().first);
    }
private:
    ReaderGui *parent;

    reader m_reader;
    std::atomic<bool> m_halted = false;

    void queue_reader_output(const reader_output &obj) {
        if (m_halted) {
            m_halted = false;
        } else {
            auto evt = new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE);
            evt->SetPayload(obj);
            wxQueueEvent(parent, evt);
        }
    }

protected:
    virtual ExitCode Entry() override {
        while (parent->m_running) {
            pdf_document doc;
            std::filesystem::path relative_path;
            try {
                doc.open(parent->m_queue.dequeue());
                relative_path = std::filesystem::relative(doc.filename(), parent->m_selected_dir);
                m_reader.set_document(doc);
                m_reader.start();

                queue_reader_output({m_reader.get_values(), relative_path, m_reader.get_warnings()});
            } catch (queue_timeout) {
                // ignore
            } catch (const layout_error &error) {
                queue_reader_output({{}, relative_path, {error.what()}});
            } catch (...) {
                queue_reader_output({{}, relative_path, {"Errore sconosciuto"}});
            }
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
    menu_reader->Append(MENU_RESTART, "Riavvia");

    menu_bar->Append(menu_reader, "Reader");
    SetMenuBar(menu_bar);

    m_table = new VariableMapTable(this);
    Show();

    Connect(wxEVT_COMMAND_READ_COMPLETE, wxThreadEventHandler(ReaderGui::OnReadCompleted));
    
    m_running = true;
    for (auto &thread : m_threads) {
        thread = new ReaderThread(this);
        thread->Run();
    }
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
        setDirectory(diag.GetPath().ToStdString());
    }
}

void ReaderGui::setDirectory(const std::filesystem::path &path) {
    if (path.empty()) return;

    m_selected_dir = path;

    for (auto &thread : m_threads) {
        dynamic_cast<ReaderThread *>(thread)->compile_layout();
    }
    m_table->DeleteAllItems();
    m_queue.clear();
    std::filesystem::recursive_directory_iterator it(path);
    for (const auto &path : it) {
        const auto &filename = path.path();
        if (std::filesystem::is_regular_file(filename) && filename.extension() == ".pdf") {
            m_queue.enqueue(filename);
        }
    }
}

void ReaderGui::OnSetScript(wxCommandEvent &evt) {
    if (getControlScript(true).second) {
        setDirectory(m_selected_dir);
    }
}

void ReaderGui::OnRestart(wxCommandEvent &evt) {
    setDirectory(m_selected_dir);
}

std::pair<std::filesystem::path, bool> ReaderGui::getControlScript(bool open_dialog) {
    wxString filename = m_config->Read("ControlScriptFilename");
    if (filename.empty() || open_dialog) {
        wxFileDialog diag(this, "Apri script di controllo", filename, wxEmptyString, "File bls (*.bls)|*.bls|Tutti i file (*.*)|*.*");

        if (diag.ShowModal() == wxID_OK) {
            filename = diag.GetPath();
            m_config->Write("ControlScriptFilename", filename);
        }
        return {filename.ToStdString(), true};
    }
    return {filename.ToStdString(), false};
}