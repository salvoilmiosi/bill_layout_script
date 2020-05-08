#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>

#include "image_panel.h"
#include "xpdf.h"

enum {
    FIRST = 10000,
    
    MENU_NEW, MENU_OPEN, MENU_SAVE, MENU_SAVEAS, MENU_CLOSE,
    MENU_UNDO, MENU_REDO, MENU_CUT, MENU_COPY, MENU_PASTE,

    TOOL_NEW, TOOL_OPEN, TOOL_SAVE,
    TOOL_UNDO, TOOL_REDO, TOOL_CUT, TOOL_COPY, TOOL_PASTE,

    CTL_LOAD_PDF, CTL_PAGE, CTL_SCALE,

    TOOL_SELECT, TOOL_NEWBOX, TOOL_DELETEBOX,
    
    CTL_LIST_BOXES,
};

class MainApp : public wxApp {
public:
    virtual bool OnInit();

private:
    void OnNewFile      (wxCommandEvent &evt);
    void OnOpenFile     (wxCommandEvent &evt);
    void OnSaveFile     (wxCommandEvent &evt);
    void OnSaveFileAs   (wxCommandEvent &evt);
    void OnClose        (wxCommandEvent &evt);
    void OnUndo         (wxCommandEvent &evt);
    void OnRedo         (wxCommandEvent &evt);
    void OnCut          (wxCommandEvent &evt);
    void OnCopy         (wxCommandEvent &evt);
    void OnPaste        (wxCommandEvent &evt);
    void OnLoadPdf      (wxCommandEvent &evt);
    void OnPageSelect   (wxCommandEvent &evt);
    void OnScaleChange  (wxCommandEvent &evt);
    void OnChangeTool   (wxCommandEvent &evt);

    DECLARE_EVENT_TABLE()

private:
    wxFrame *m_frame;
    wxImagePanel *m_image;

    wxComboBox *m_page;
    wxSlider *m_scale;

    wxListCtrl *m_list_boxes;

private:
    std::string app_path;
    xpdf::pdf_info info;
    std::string pdf_filename{};
    int selected_tool = TOOL_SELECT;
};
wxIMPLEMENT_APP(MainApp);

BEGIN_EVENT_TABLE(MainApp, wxApp)
    EVT_MENU        (MENU_NEW,      MainApp::OnNewFile)
    EVT_TOOL        (TOOL_NEW,      MainApp::OnNewFile)
    EVT_MENU        (MENU_OPEN,     MainApp::OnOpenFile)
    EVT_TOOL        (TOOL_OPEN,     MainApp::OnOpenFile)
    EVT_MENU        (MENU_SAVE,     MainApp::OnSaveFile)
    EVT_TOOL        (TOOL_SAVE,     MainApp::OnSaveFile)
    EVT_MENU        (MENU_SAVEAS,   MainApp::OnSaveFileAs)
    EVT_MENU        (MENU_CLOSE,    MainApp::OnClose)
    EVT_MENU        (MENU_UNDO,     MainApp::OnUndo)
    EVT_TOOL        (TOOL_UNDO,     MainApp::OnUndo)
    EVT_MENU        (MENU_REDO,     MainApp::OnRedo)
    EVT_TOOL        (TOOL_REDO,     MainApp::OnRedo)
    EVT_MENU        (MENU_CUT,      MainApp::OnCut)
    EVT_TOOL        (TOOL_CUT,      MainApp::OnCut)
    EVT_MENU        (MENU_COPY,     MainApp::OnCopy)
    EVT_TOOL        (TOOL_COPY,     MainApp::OnCopy)
    EVT_MENU        (MENU_PASTE,    MainApp::OnPaste)
    EVT_TOOL        (TOOL_PASTE,    MainApp::OnPaste)
    EVT_BUTTON      (CTL_LOAD_PDF,  MainApp::OnLoadPdf)
    EVT_COMBOBOX    (CTL_PAGE,      MainApp::OnPageSelect)
    EVT_TEXT_ENTER  (CTL_PAGE,      MainApp::OnPageSelect)
    EVT_SLIDER      (CTL_SCALE,     MainApp::OnScaleChange)
    EVT_TOOL        (TOOL_SELECT,   MainApp::OnChangeTool)
    EVT_TOOL        (TOOL_NEWBOX,   MainApp::OnChangeTool)
    EVT_TOOL        (TOOL_DELETEBOX,MainApp::OnChangeTool)
END_EVENT_TABLE()

bool MainApp::OnInit() {
    wxInitAllImageHandlers();

    wxFileName f(wxStandardPaths::Get().GetExecutablePath());
    app_path = f.GetPath().ToStdString();

    m_frame = new wxFrame(nullptr, wxID_ANY, "Layout Bolletta", wxDefaultPosition, wxSize(900, 800));

    wxMenuBar *menuBar = new wxMenuBar();

    wxMenu *menuFile = new wxMenu;
    menuFile->Append(MENU_NEW, "&Nuovo\tCtrl-N", "Crea un Nuovo Layout");
    menuFile->Append(MENU_OPEN, "&Apri...\tCtrl-O", "Apri un Layout");
    menuFile->Append(MENU_SAVE, "&Salva\tCtrl-S", "Salva il Layout");
    menuFile->Append(MENU_SAVEAS, "Sa&lva con nome...\tCtrl-Shift-S", "Salva il Layout con nome...");
    menuFile->AppendSeparator();
    menuFile->Append(MENU_CLOSE, "&Chiudi\tCtrl-W", "Chiudi la finestra");
    menuBar->Append(menuFile, "&File");

    wxMenu *menuEdit = new wxMenu;
    menuEdit->Append(MENU_UNDO, "&Annulla\tCtrl-Z", "Annulla l'ultima operazione");
    menuEdit->Append(MENU_REDO, "&Ripeti\tCtrl-Y", "Ripeti l'ultima operazione");
    menuEdit->AppendSeparator();
    menuEdit->Append(MENU_CUT, "&Taglia\tCtrl-X", "Taglia la selezione");
    menuEdit->Append(MENU_COPY, "&Copia\tCtrl-C", "Copia la selezione");
    menuEdit->Append(MENU_PASTE, "&Incolla\tCtrl-V", "Incolla nella selezione");
    menuBar->Append(menuEdit, "&Modifica");

    m_frame->SetMenuBar(menuBar);

    wxToolBar *toolbar_top = m_frame->CreateToolBar();

    toolbar_top->AddTool(TOOL_NEW, "Nuovo", wxArtProvider::GetBitmap(wxART_NEW), "Nuovo");
    toolbar_top->AddTool(TOOL_OPEN, "Apri", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Apri");
    toolbar_top->AddTool(TOOL_SAVE, "Salva", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Salva");

    toolbar_top->AddSeparator();

    toolbar_top->AddTool(TOOL_UNDO, "Annulla", wxArtProvider::GetBitmap(wxART_UNDO), "Annulla");
    toolbar_top->AddTool(TOOL_REDO, "Ripeti", wxArtProvider::GetBitmap(wxART_REDO), "Ripeti");

    toolbar_top->AddSeparator();

    toolbar_top->AddTool(TOOL_CUT, "Taglia", wxArtProvider::GetBitmap(wxART_CUT), "Taglia");
    toolbar_top->AddTool(TOOL_COPY, "Copia", wxArtProvider::GetBitmap(wxART_COPY), "Copia");
    toolbar_top->AddTool(TOOL_PASTE, "Incolla", wxArtProvider::GetBitmap(wxART_PASTE), "Incolla");

    toolbar_top->AddSeparator();

    toolbar_top->AddStretchableSpace();

    wxButton *btn_load_pdf = new wxButton(toolbar_top, CTL_LOAD_PDF, "Carica PDF", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(btn_load_pdf, "Carica un file PDF");

    m_page = new wxComboBox(toolbar_top, CTL_PAGE, "Pagina", wxDefaultPosition, wxSize(70, -1));
    toolbar_top->AddControl(m_page, "Pagina");

    m_scale = new wxSlider(toolbar_top, CTL_SCALE, 50, 1, 100, wxDefaultPosition, wxSize(200, -1));
    toolbar_top->AddControl(m_scale, "Scala");

    toolbar_top->Realize();

    wxSplitterWindow *m_splitter = new wxSplitterWindow(m_frame);

    wxPanel *m_panel_left = new wxPanel(m_splitter);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);

    wxToolBar *toolbar_side = new wxToolBar(m_panel_left, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_VERTICAL);

    toolbar_side->AddRadioTool(TOOL_SELECT, "Seleziona", wxBitmap("IDT_SELECT", wxBITMAP_TYPE_PNG_RESOURCE), wxNullBitmap, "Seleziona");
    toolbar_side->AddRadioTool(TOOL_NEWBOX, "Nuovo rettangolo", wxBitmap("IDT_NEWBOX", wxBITMAP_TYPE_PNG_RESOURCE), wxNullBitmap, "Nuovo rettangolo");
    toolbar_side->AddRadioTool(TOOL_DELETEBOX, "Cancella rettangolo", wxBitmap("IDT_DELETEBOX", wxBITMAP_TYPE_PNG_RESOURCE), wxNullBitmap, "Cancella rettangolo");

    toolbar_side->Realize();
    sizer->Add(toolbar_side, 0, wxEXPAND);

    m_list_boxes = new wxListCtrl(m_panel_left, CTL_LIST_BOXES);
    sizer->Add(m_list_boxes, 1, wxEXPAND);

    m_panel_left->SetSizer(sizer);
    m_image = new wxImagePanel(m_splitter);

    m_splitter->SplitVertically(m_panel_left, m_image, 200);
    m_splitter->SetMinimumPaneSize(100);
    m_frame->Show();
    return true;
}

void MainApp::OnNewFile(wxCommandEvent &evt) {

}

void MainApp::OnOpenFile(wxCommandEvent &evt) {

}

void MainApp::OnSaveFile(wxCommandEvent &evt) {

}

void MainApp::OnSaveFileAs(wxCommandEvent &evt) {

}

void MainApp::OnClose(wxCommandEvent &evt) {
    m_frame->Close();
}

void MainApp::OnUndo(wxCommandEvent &evt) {

}

void MainApp::OnRedo(wxCommandEvent &evt) {
    
}

void MainApp::OnCut(wxCommandEvent &evt) {
    
}

void MainApp::OnCopy(wxCommandEvent &evt) {
    
}

void MainApp::OnPaste(wxCommandEvent &evt) {
    
}

void MainApp::OnLoadPdf(wxCommandEvent &evt) {
    wxFileDialog diag(m_frame, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    try {
        pdf_filename = diag.GetPath().ToStdString();
        info = xpdf::pdf_get_info(app_path, pdf_filename);
        m_page->Clear();
        for (int i=1; i<=info.num_pages; ++i) {
            m_page->Append(wxString::Format("%i", i));
        }
        m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, 1));
        m_page->Fit();
        m_page->SetSelection(0);
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void MainApp::OnPageSelect(wxCommandEvent &evt) {
    if (pdf_filename.empty()) return;

    try {
        int selected_page = std::stoi(m_page->GetValue().ToStdString());
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
    m_image->rescale(m_scale->GetValue() / 100.f);
}

void MainApp::OnChangeTool(wxCommandEvent &evt) {
    selected_tool = evt.GetId();
}