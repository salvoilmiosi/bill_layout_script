#include <wx/app.h>

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;
};
wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    return true;
}