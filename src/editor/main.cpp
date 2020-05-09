#include "main.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/splitter.h>

#include "pdf_to_image.h"
#include "box_dialog.h"

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
    EVT_LISTBOX     (CTL_LIST_BOXES,MainApp::OnSelectBox)
    EVT_LISTBOX_DCLICK (CTL_LIST_BOXES, MainApp::EditSelectedBox)
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

    m_page = new wxComboBox(toolbar_top, CTL_PAGE, "Pagina", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(m_page, "Pagina");

    m_scale = new wxSlider(toolbar_top, CTL_SCALE, 50, 1, 100, wxDefaultPosition, wxSize(200, -1));
    toolbar_top->AddControl(m_scale, "Scala");

    toolbar_top->Realize();

    wxSplitterWindow *m_splitter = new wxSplitterWindow(m_frame);

    wxPanel *m_panel_left = new wxPanel(m_splitter);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);

    wxToolBar *toolbar_side = new wxToolBar(m_panel_left, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_VERTICAL);

    auto loadIcon = [](const char *name) {
        wxBitmap icon(name, wxBITMAP_TYPE_PNG_RESOURCE);
        if (icon.IsOk()) {
            return icon;
        } else {
            return wxArtProvider::GetBitmap(wxART_MISSING_IMAGE);
        }
    };

    toolbar_side->AddRadioTool(TOOL_SELECT, "Seleziona", loadIcon("IDT_SELECT"), wxNullBitmap, "Seleziona");
    toolbar_side->AddRadioTool(TOOL_NEWBOX, "Nuovo rettangolo", loadIcon("IDT_NEWBOX"), wxNullBitmap, "Nuovo rettangolo");
    toolbar_side->AddRadioTool(TOOL_DELETEBOX, "Cancella rettangolo", loadIcon("IDT_DELETEBOX"), wxNullBitmap, "Cancella rettangolo");

    toolbar_side->Realize();
    sizer->Add(toolbar_side, 0, wxEXPAND);

    m_list_boxes = new wxListBox(m_panel_left, CTL_LIST_BOXES);
    sizer->Add(m_list_boxes, 1, wxEXPAND);

    m_panel_left->SetSizer(sizer);
    m_image = new box_editor_panel(m_splitter, this);

    m_splitter->SplitVertically(m_panel_left, m_image, 200);
    m_splitter->SetMinimumPaneSize(100);
    m_frame->Show();
    return true;
}

void MainApp::OnNewFile(wxCommandEvent &evt) {
    layout_filename.clear();
    layout.newFile();
    updateLayout();
}

void MainApp::OnOpenFile(wxCommandEvent &evt) {
    wxFileDialog diag(m_frame, "Apri Layout Bolletta", wxEmptyString, wxEmptyString, "File bolletta (*.bolletta)|*.bolletta|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    try {
        layout_filename = diag.GetPath().ToStdString();
        layout.openFile(layout_filename);
        updateLayout();
    } catch (layout_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void MainApp::OnSaveFile(wxCommandEvent &evt) {
    if (layout_filename.empty()) {
        OnSaveFileAs(evt);
    } else {
        try {
            layout.saveFile(layout_filename);
        } catch (layout_error &error) {
            wxMessageBox(error.message, "Errore", wxICON_ERROR);
        }
    }
}

void MainApp::OnSaveFileAs(wxCommandEvent &evt) {
    wxFileDialog diag(m_frame, "Salva Layout Bolletta", wxEmptyString, layout_filename, "File bolletta (*.bolletta)|*.bolletta|Tutti i file (*.*)|*.*", wxFD_SAVE);

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    try {
        layout_filename = diag.GetPath().ToStdString();
        layout.saveFile(layout_filename);
    } catch (layout_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
    }
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
        m_page->SetSelection(0);
        selected_page = 1;
        m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, 1));
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void MainApp::setSelectedPage(int page) {
    if (page == selected_page) return;

    m_page->SetValue(std::to_string(page));
    try {
        selected_page = page;
        m_image->setImage(xpdf::pdf_to_image(app_path, pdf_filename, page));
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void MainApp::OnPageSelect(wxCommandEvent &evt) {
    if (pdf_filename.empty()) return;

    try {
        int page = std::stoi(m_page->GetValue().ToStdString());
        if (page > info.num_pages || page <= 0) {
            wxBell();
        } else {
            setSelectedPage(page);
        }
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

void MainApp::updateLayout() {
    m_list_boxes->Clear();
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        auto &box = layout.boxes[i];
        m_list_boxes->Append(box.name);
        if (box.selected) {
            m_list_boxes->SetSelection(i);
        }
    }
    m_image->paintNow();
}

void MainApp::OnSelectBox(wxCommandEvent &evt) {
    size_t selection = m_list_boxes->GetSelection();
    selectBox(selection);
}

void MainApp::selectBox(int selection) {
    m_list_boxes->SetSelection(selection);
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        layout.boxes[i].selected = (int)i == selection;
    }
    if (selection >= 0 && selection < (int) layout.boxes.size()) {
        setSelectedPage(layout.boxes[selection].page);
    }
    m_image->paintNow();
}

void MainApp::EditSelectedBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    auto &box = layout.boxes[selection];
    box_dialog diag(m_frame, box);

    if (diag.ShowModal() == wxID_OK) {
        updateLayout();
    }
}