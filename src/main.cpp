#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "image_panel.h"
#include "xpdf.h"

static constexpr wxWindowID BUTTON1 = 10001;
static constexpr wxWindowID DROPDOWN = 10002;
static constexpr wxWindowID SLIDER = 10003;

class MainApp : public wxApp {
public:
    virtual bool OnInit();

private:
    void OnButtonClick(wxCommandEvent &evt);
    void OnPageSelect(wxCommandEvent &evt);
    void OnScaleChange(wxCommandEvent &evt);

private:
    wxFrame *m_frame;
    wxImagePanel *m_image;

    wxChoice *dropdown;
    wxSlider *scale;

    std::string app_path;
    std::string pdf_filename{};

    DECLARE_EVENT_TABLE()
};
wxIMPLEMENT_APP(MainApp);

BEGIN_EVENT_TABLE(MainApp, wxApp)
    EVT_BUTTON(BUTTON1, MainApp::OnButtonClick)
    EVT_CHOICE(DROPDOWN, MainApp::OnPageSelect)
    EVT_SLIDER(SLIDER, MainApp::OnScaleChange)
END_EVENT_TABLE()

bool MainApp::OnInit() {
    wxInitAllImageHandlers();

    wxFileName f(wxStandardPaths::Get().GetExecutablePath());
    app_path = f.GetPath().ToStdString();

    m_frame = new wxFrame(nullptr, wxID_ANY, "PDF Viewer", wxDefaultPosition, wxSize(800, 600));

    wxPanel *m_panel = new wxPanel(m_frame, wxID_ANY);

    wxBoxSizer *top_level = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *header = new wxBoxSizer(wxHORIZONTAL);

    wxButton *btn = new wxButton(m_panel, BUTTON1, "Carica PDF", wxDefaultPosition, wxSize(200, 30));
    header->Add(btn, 0, wxALL, 10);

    wxString default_choices[] = {"Seleziona Pagina"};
    dropdown = new wxChoice(m_panel, DROPDOWN, wxDefaultPosition, wxSize(150, 30), 1, default_choices);
    dropdown->SetSelection(0);
    header->Add(dropdown, 0, wxALL, 10);

    scale = new wxSlider(m_panel, SLIDER, 100, 1, 100, wxDefaultPosition, wxSize(200, 30));
    header->Add(scale, 0, wxALL, 10);

    top_level->Add(header, 0, wxALL);

    m_image = new wxImagePanel(m_panel);
    top_level->Add(m_image, 1, wxALL | wxEXPAND, 0);

    m_panel->SetSizer(top_level);
    m_frame->Show();
    return true;
}

void MainApp::OnButtonClick(wxCommandEvent &evt) {
    wxFileDialog diag(m_frame, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    try {
        pdf_filename = diag.GetPath().ToStdString();
        xpdf::pdf_info info = xpdf::pdf_get_info(app_path, pdf_filename);
        dropdown->Clear();
        for (int i=1; i<=info.num_pages; ++i) {
            dropdown->Append(wxString::Format("%i", i));
        }
        m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, 1));
        dropdown->Fit();
        dropdown->SetSelection(0);
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void MainApp::OnPageSelect(wxCommandEvent &evt) {
    if (pdf_filename.empty()) return;

    try {
        int selected_page = dropdown->GetSelection() + 1;
        m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, selected_page));
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    } catch(const std::invalid_argument &error) {

    }
}

void MainApp::OnScaleChange(wxCommandEvent &evt) {
    m_image->rescale(scale->GetValue() / 100.f);
}