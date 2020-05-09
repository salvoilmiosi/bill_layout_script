#include "editor.h"

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;

private:
    frame_editor *editor;
};
wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    wxImage::AddHandler(new wxPNGHandler);

    editor = new frame_editor();
    return true;
}