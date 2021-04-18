#ifndef __READERGUI_H__
#define __READERGUI_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/config.h>

#include "dati_fattura.h"
#include "safe_queue.h"

#include <array>
#include <atomic>

#include "reader.h"
#include "layout.h"

class ReaderThread : public wxThread {
public:
    ReaderThread(class ReaderGui *parent);
    ~ReaderThread() {
        abort();
    }

    void start();
    void abort();

private:
    class ReaderGui *parent;
    reader m_reader;

protected:
    virtual ExitCode Entry() override;
};

class ReaderGui : public wxFrame {
public:
    ReaderGui();
    ~ReaderGui();

private:
    DECLARE_EVENT_TABLE()

    VariableMapTable *m_table;
    wxConfig *m_config;
    
    std::filesystem::path m_selected_dir;

    safe_queue<std::filesystem::path> m_queue;
    std::array<ReaderThread *, 16> m_threads{nullptr};
    std::atomic<bool> m_running;

    void OnReadCompleted(wxThreadEvent &evt);

    void OnOpenFolder(wxCommandEvent &evt);
    void OnSetScript(wxCommandEvent &evt);
    void OnRestart(wxCommandEvent &evt);
    void OnStop(wxCommandEvent &evt);
    void OnClear(wxCommandEvent &evt);

    void startReader(const std::filesystem::path &path);
    void stopReader();
    std::pair<std::filesystem::path, bool> getControlScript(bool open_dialog = false);

    friend class ReaderThread;
};

#endif