#ifndef __READERGUI_H__
#define __READERGUI_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

class ReaderGui : public wxFrame {
public:
    ReaderGui();

private:
    DECLARE_EVENT_TABLE()
};

#endif