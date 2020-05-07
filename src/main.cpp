#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>

#include "image_panel.h"
#include "xpdf.h"

enum {
    LOAD_PDF = 10001,
    DROPDOWN = 10002,
    SLIDER = 10003,
    LIST_BOXES = 10004,
    TOOL_NEW = 10005,
    TOOL_OPEN = 10006,
    TOOL_SAVE = 10007,
    TOOL_SAVEAS = 10008,
    TOOL_UNDO = 10009,
    TOOL_REDO = 10010,
    TOOL_CUT = 10011,
    TOOL_COPY = 10012,
    TOOL_PASTE = 10013,
};

class MainApp : public wxApp {
public:
    virtual bool OnInit();

private:
    void OnLoadPdf(wxCommandEvent &evt);
    void OnPageSelect(wxCommandEvent &evt);
    void OnScaleChange(wxCommandEvent &evt);

private:
    wxFrame *m_frame;
    wxImagePanel *m_image;

    wxComboBox *dropdown;
    wxSlider *scale;

    wxListCtrl *list_boxes;

    std::string app_path;
    xpdf::pdf_info info;
    std::string pdf_filename{};

    DECLARE_EVENT_TABLE()
};
wxIMPLEMENT_APP(MainApp);

BEGIN_EVENT_TABLE(MainApp, wxApp)
    EVT_BUTTON      (LOAD_PDF,      MainApp::OnLoadPdf)
    EVT_COMBOBOX    (DROPDOWN,      MainApp::OnPageSelect)
    EVT_TEXT_ENTER  (DROPDOWN,      MainApp::OnPageSelect)
    EVT_SLIDER      (SLIDER,        MainApp::OnScaleChange)
END_EVENT_TABLE()

bool MainApp::OnInit() {
    wxInitAllImageHandlers();

    wxFileName f(wxStandardPaths::Get().GetExecutablePath());
    app_path = f.GetPath().ToStdString();

    m_frame = new wxFrame(nullptr, wxID_ANY, "PDF Viewer", wxDefaultPosition, wxSize(800, 600));

    wxToolBar *toolbar = m_frame->CreateToolBar();

    toolbar->AddTool(TOOL_NEW, "Nuovo", wxArtProvider::GetBitmap(wxART_NEW), "Nuovo");
    toolbar->AddTool(TOOL_OPEN, "Apri", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Apri");
    toolbar->AddTool(TOOL_SAVE, "Salva", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Salva");
    toolbar->AddTool(TOOL_SAVEAS, "Salva con nome", wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS), "Salva con nome");

    toolbar->AddSeparator();

    toolbar->AddTool(TOOL_UNDO, "Annulla", wxArtProvider::GetBitmap(wxART_UNDO), "Annulla");
    toolbar->AddTool(TOOL_REDO, "Ripeti", wxArtProvider::GetBitmap(wxART_REDO), "Ripeti");

    toolbar->AddSeparator();

    toolbar->AddTool(TOOL_CUT, "Taglia", wxArtProvider::GetBitmap(wxART_CUT), "Taglia");
    toolbar->AddTool(TOOL_COPY, "Copia", wxArtProvider::GetBitmap(wxART_COPY), "Copia");
    toolbar->AddTool(TOOL_PASTE, "Incolla", wxArtProvider::GetBitmap(wxART_PASTE), "Incolla");

    toolbar->AddSeparator();

    toolbar->AddStretchableSpace();

    wxButton *button = new wxButton(toolbar, LOAD_PDF, "Carica PDF", wxDefaultPosition, wxSize(100, -1));
    toolbar->AddControl(button, "Carica un file PDF");

    dropdown = new wxComboBox(toolbar, DROPDOWN, "Pagina", wxDefaultPosition, wxSize(70, -1));
    toolbar->AddControl(dropdown, "Pagina");

    scale = new wxSlider(toolbar, SLIDER, 100, 1, 100, wxDefaultPosition, wxSize(200, -1));
    toolbar->AddControl(scale, "Scala");

    toolbar->Realize();

    wxSplitterWindow *m_splitter = new wxSplitterWindow(m_frame);

    list_boxes = new wxListCtrl(m_splitter, LIST_BOXES);

    m_image = new wxImagePanel(m_splitter);

    m_splitter->SplitVertically(list_boxes, m_image, 200);
    m_splitter->SetMinimumPaneSize(20);
    m_frame->Show();
    return true;
}

void MainApp::OnLoadPdf(wxCommandEvent &evt) {
    wxFileDialog diag(m_frame, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    try {
        pdf_filename = diag.GetPath().ToStdString();
        info = xpdf::pdf_get_info(app_path, pdf_filename);
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
        int selected_page = std::stoi(dropdown->GetValue().ToStdString());
        if (selected_page > info.num_pages || selected_page <= 0) {
            wxBell();
        } else {
            m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, selected_page));
        }
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    } catch(const std::invalid_argument &error) {
        wxBell();
    }
}

void MainApp::OnScaleChange(wxCommandEvent &evt) {
    m_image->rescale(scale->GetValue() / 100.f);
}