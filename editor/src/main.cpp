#include "editor.h"

#include <wx/cmdline.h>

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    frame_editor *editor;

    wxLocale loc = wxLANGUAGE_DEFAULT;

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

static const wxCmdLineEntryDesc g_cmdline_desc[] = {
    { wxCMD_LINE_PARAM, nullptr, nullptr, "input file", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
    { wxCMD_LINE_NONE }
};

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.SetDesc(g_cmdline_desc);
    parser.SetSwitchChars('-');
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) {
        filename = parser.GetParam(0).ToStdString();
    }
    return true;
}