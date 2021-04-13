#include "readergui.h"

#include <filesystem>

#include <wx/sizer.h>
#include <wx/thread.h>

#include "reader.h"

wxDEFINE_EVENT(wxEVT_COMMAND_READ_COMPLETE, wxThreadEvent);

BEGIN_EVENT_TABLE(ReaderGui, wxFrame)
END_EVENT_TABLE()

wxThread::ExitCode ReaderThread::Entry() {
    reader my_reader;
    my_reader.add_flags(reader_flags::RECURSIVE);
    my_reader.add_layout("C:/dev/layoutbolletta/layouts/controllo.bls");

    while (true) {
        pdf_document doc(m_queue->dequeue());
        my_reader.set_document(doc);
        my_reader.start();

        auto evt = new wxThreadEvent(wxEVT_COMMAND_READ_COMPLETE);
        evt->SetPayload(my_reader.get_values());
        evt->SetString(doc.filename().string());
        wxQueueEvent(parent, evt);
    }

    return 0;
}

ReaderGui::ReaderGui() : wxFrame(nullptr, wxID_ANY, "Reader GUI", wxDefaultPosition, wxSize(900, 700)) {
    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);

    m_table = new VariableMapTable(this);
    top_level->Add(m_table, 1, wxEXPAND | wxALL, 5);

    SetSizer(top_level);
    Show();

    Connect(wxEVT_COMMAND_READ_COMPLETE, wxThreadEventHandler(ReaderGui::OnReadCompleted));
    
    for (auto* &thread : m_threads) {
        thread = new ReaderThread(this, &m_queue);
        thread->Run();
    }

    std::filesystem::recursive_directory_iterator it("C:/dev/layoutbolletta/work/fatture");
    for (const auto &path : it) {
        const auto &filename = path.path();
        if (std::filesystem::is_regular_file(filename) && filename.extension() == ".pdf") {
            m_queue.enqueue(filename);
        }
    }
}

ReaderGui::~ReaderGui() {
    for (auto *thread : m_threads) {
        delete thread;
    }
}

void ReaderGui::OnReadCompleted(wxThreadEvent &evt) {
    m_table->AddReaderValues(evt.GetString().ToStdString(), evt.GetPayload<variable_map>());
    m_table->Refresh();
}