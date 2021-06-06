#include "editor.h"

#include <wx/cmdline.h>

#include "intl.h"

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;
    virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
    frame_editor *editor;

    wxString bls_filename;
    wxString pdf_filename;
};
wxIMPLEMENT_APP(MainApp);

bool MainApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    intl::set_language(wxLANGUAGE_DEFAULT);

    wxImage::AddHandler(new wxPNGHandler);

    editor = new frame_editor();

    if (!bls_filename.empty()) {
        editor->openFile(bls_filename);
    }
    if (!pdf_filename.empty()) {
        editor->loadPdf(pdf_filename);
    }

    SetTopWindow(editor);
    return true;
}

void MainApp::OnInitCmdLine(wxCmdLineParser &parser) {
    parser.AddParam("input-bls", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    parser.AddOption("p", "input-pdf", "Open PDF File");
}

bool MainApp::OnCmdLineParsed(wxCmdLineParser &parser) {
    if (parser.GetParamCount() >= 1) {
        bls_filename = parser.GetParam(0);
    }
    parser.Found("p", &pdf_filename);

    return true;
}