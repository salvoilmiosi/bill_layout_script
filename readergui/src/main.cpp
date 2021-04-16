#include <wx/app.h>

#include "gui/readergui.h"

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;

private:
    ReaderGui *main_gui;

    intl::locale loc{wxLANGUAGE_DEFAULT};
};
wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }
    
    main_gui = new ReaderGui();

    SetTopWindow(main_gui);

    return true;
}