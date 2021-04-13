#ifndef __READERGUI_H__
#define __READERGUI_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "dati_fattura.h"
#include "safe_queue.h"

typedef SafeQueue<std::filesystem::path> file_queue;

class ReaderThread : public wxThread {
public:
    ReaderThread(wxEvtHandler *parent, file_queue *queue) : parent(parent), m_queue(queue) {}

protected:
    file_queue *m_queue;
    wxEvtHandler *parent;
    
    virtual ExitCode Entry() override;
};

class ReaderGui : public wxFrame {
public:
    ReaderGui();
    ~ReaderGui();

private:
    DECLARE_EVENT_TABLE()

    VariableMapTable *m_table;
    
    file_queue m_queue;

    ReaderThread *m_threads[4];

    void OnReadCompleted(wxThreadEvent &evt);
};

#endif