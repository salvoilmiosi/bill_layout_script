#include <wx/app.h>
#include <wx/intl.h>

#include "gui/readergui.h"

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;

private:
    ReaderGui *main_gui;

    wxLocale locale{wxLANGUAGE_ITALIAN};
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