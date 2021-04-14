#ifndef __READERGUI_H__
#define __READERGUI_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/config.h>

#include "dati_fattura.h"
#include "safe_queue.h"

#include <memory>
#include <array>
#include <atomic>

class ReaderGui : public wxFrame {
public:
    ReaderGui();
    ~ReaderGui();

private:
    DECLARE_EVENT_TABLE()

    VariableMapTable *m_table;
    wxConfig *m_config;
    
    safe_queue<std::filesystem::path> m_queue;

    std::array<wxThread*, 4> m_threads{nullptr};
    std::atomic<bool> m_running;

    void OnReadCompleted(wxThreadEvent &evt);

    void OnOpenFolder(wxCommandEvent &evt);
    void OnSetScript(wxCommandEvent &evt);

    void startReaderThreads();
    std::filesystem::path getControlScript(bool open_dialog = false);

    friend class ReaderThread;
};

#endif