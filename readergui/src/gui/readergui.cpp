#include "readergui.h"

#include <filesystem>

#include <wx/sizer.h>
#include <wx/thread.h>

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);

enum {
    MENU_OPEN_FOLDER = 10000,
    MENU_SET_SCRIPT,
    MENU_RESTART,
    MENU_STOP
};

BEGIN_EVENT_TABLE(ReaderGui, wxFrame)
    EVT_MENU(MENU_OPEN_FOLDER,  ReaderGui::OnOpenFolder)
    EVT_MENU(MENU_SET_SCRIPT,   ReaderGui::OnSetScript)
    EVT_MENU(MENU_RESTART,      ReaderGui::OnRestart)
    EVT_MENU(MENU_STOP,         ReaderGui::OnStop)
END_EVENT_TABLE()

ReaderThread::ReaderThread(ReaderGui *parent) : wxThread(wxTHREAD_JOINABLE), parent(parent) {}

void ReaderThread::start() {
    m_reader.add_flags(reader_flags::RECURSIVE);
    m_reader.add_layout(parent->getControlScript().first);
    m_running = true;
    Run();
}

void ReaderThread::abort() {
    m_running = false;
    m_reader.abort();
    Wait();
}

wxThread::ExitCode ReaderThread::Entry() {
    auto queue_reader_output = [&](const reader_output &obj) {
        auto evt = new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE);
        evt->SetPayload(obj);
        wxQueueEvent(parent, evt);
    };

    while (m_running && ! parent->m_queue.empty()) {
        std::filesystem::path relative_path;
        try {
            pdf_document doc(parent->m_queue.dequeue());
            relative_path = std::filesystem::relative(doc.filename(), parent->m_selected_dir);
            m_reader.set_document(doc);
            m_reader.start();

            queue_reader_output({m_reader.get_values(), relative_path, m_reader.get_warnings()});
        } catch (reader_aborted) {
            break;
        } catch (const layout_error &error) {
            queue_reader_output({{}, relative_path, {error.what()}});
        } catch (...) {
            queue_reader_output({{}, relative_path, {"Errore sconosciuto"}});
        }
    }

    return 0;
}

ReaderGui::ReaderGui() : wxFrame(nullptr, wxID_ANY, "Reader GUI", wxDefaultPosition, wxSize(900, 700)) {
    m_config = new wxConfig("BillLayoutScript");

    wxMenuBar *menu_bar = new wxMenuBar;

    wxMenu *menu_reader = new wxMenu;
    menu_reader->Append(MENU_OPEN_FOLDER, "Apri Cartella");
    menu_reader->Append(MENU_SET_SCRIPT, "Imposta Script Layout");
    menu_reader->Append(MENU_RESTART, "Riavvia");
    menu_reader->Append(MENU_STOP, "Stop");

    menu_bar->Append(menu_reader, "Reader");
    SetMenuBar(menu_bar);

    m_table = new VariableMapTable(this);
    Show();

    Connect(wxEVT_COMMAND_READ_COMPLETE, wxThreadEventHandler(ReaderGui::OnReadCompleted));
}

ReaderGui::~ReaderGui() {
    stopReader();
}

void ReaderGui::OnReadCompleted(wxThreadEvent &evt) {
    m_table->AddReaderValues(evt.GetPayload<reader_output>());
    m_table->Refresh();
}

void ReaderGui::OnOpenFolder(wxCommandEvent &evt) {
    wxDirDialog diag(this, "Scegli una cartella...");
    if (diag.ShowModal() == wxID_OK) {
        startReader(diag.GetPath().ToStdString());
    }
}

void ReaderGui::startReader(const std::filesystem::path &path) {
    if (path.empty()) return;

    m_selected_dir = path;

    stopReader();

    m_queue.clear();
    for (const auto &path : std::filesystem::recursive_directory_iterator(path)) {
        const auto &filename = path.path();
        if (std::filesystem::is_regular_file(filename) && filename.extension() == ".pdf") {
            m_queue.enqueue(filename);
        }
    }

    m_table->DeleteAllItems();
    for (size_t i=0; i < std::min({m_threads.size(), m_queue.size(), size_t(wxThread::GetCPUCount())}); ++i) {
        auto &t = m_threads[i];
        t = new ReaderThread(this);
        t->start();
    }
}

void ReaderGui::stopReader() {
    for (auto &t : m_threads) {
        delete t;
        t = nullptr;
    }
}

void ReaderGui::OnSetScript(wxCommandEvent &evt) {
    if (getControlScript(true).second) {
        startReader(m_selected_dir);
    }
}

void ReaderGui::OnRestart(wxCommandEvent &evt) {
    startReader(m_selected_dir);
}

void ReaderGui::OnStop(wxCommandEvent &evt) {
    stopReader();
}

std::pair<std::filesystem::path, bool> ReaderGui::getControlScript(bool open_dialog) {
    wxString filename = m_config->Read("ControlScriptFilename");
    if (filename.empty() || open_dialog) {
        wxFileDialog diag(this, "Apri script di controllo", filename, wxEmptyString, "File bls (*.bls)|*.bls|Tutti i file (*.*)|*.*");

        if (diag.ShowModal() == wxID_OK) {
            filename = diag.GetPath();
            m_config->Write("ControlScriptFilename", filename);
            return {filename.ToStdString(), true};
        }
    }
    return {filename.ToStdString(), false};
}