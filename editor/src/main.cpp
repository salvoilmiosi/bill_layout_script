#include "editor.h"

#include <wx/cmdline.h>

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    frame_editor *editor;

    std::string filename{};
};
wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    wxImage::AddHandler(new wxPNGHandler);

    editor = new frame_editor();

    if (!filename.empty()) {
        editor->openFile(filename);
    }

    SetTopWindow(editor);
    return true;
}

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) {
        filename = parser.GetParam(0).ToStdString();
    }
    return true;
}