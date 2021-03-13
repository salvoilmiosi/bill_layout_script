#include "editor.h"

#include <fstream>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/splitter.h>

#include "resources.h"
#include "box_editor_panel.h"
#include "pdf_to_image.h"
#include "box_dialog.h"

BEGIN_EVENT_TABLE(frame_editor, wxFrame)
    EVT_MENU (MENU_NEW, frame_editor::OnNewFile)
    EVT_MENU (MENU_OPEN, frame_editor::OnOpenFile)
    EVT_MENU (MENU_SAVE, frame_editor::OnSaveFile)
    EVT_MENU (MENU_SAVEAS, frame_editor::OnSaveFileAs)
    EVT_MENU (MENU_CLOSE, frame_editor::OnMenuClose)
    EVT_MENU (MENU_UNDO, frame_editor::OnUndo)
    EVT_MENU (MENU_REDO, frame_editor::OnRedo)
    EVT_MENU (MENU_CUT, frame_editor::OnCut)
    EVT_MENU (MENU_COPY, frame_editor::OnCopy)
    EVT_MENU (MENU_PASTE, frame_editor::OnPaste)
    EVT_MENU (MENU_LOAD_PDF, frame_editor::OnLoadPdf)
    EVT_MENU_RANGE (MENU_OPEN_RECENT, MENU_OPEN_RECENT_END, frame_editor::OnOpenRecent)
    EVT_MENU_RANGE (MENU_OPEN_PDF_RECENT, MENU_OPEN_PDF_RECENT_END, frame_editor::OnOpenRecentPdf)
    EVT_MENU (MENU_EDITBOX, frame_editor::EditSelectedBox)
    EVT_MENU (MENU_DELETE, frame_editor::OnDelete)
    EVT_MENU (MENU_READDATA, frame_editor::OnReadData)
    EVT_MENU (MENU_EDITCONTROL, frame_editor::OpenControlScript)
    EVT_MENU (MENU_SETLANGUAGE, frame_editor::OnSetLanguage)
    EVT_BUTTON(CTL_AUTO_LAYOUT, frame_editor::OnAutoLayout)
    EVT_BUTTON (CTL_ROTATE, frame_editor::OnRotate)
    EVT_BUTTON (CTL_LOAD_PDF, frame_editor::OnLoadPdf)
    EVT_SPINCTRL (CTL_PAGE, frame_editor::OnPageSelect)
    EVT_TEXT_ENTER (CTL_PAGE, frame_editor::OnPageEnter)
    EVT_COMMAND_SCROLL_THUMBTRACK (CTL_SCALE, frame_editor::OnScaleChange)
    EVT_COMMAND_SCROLL_CHANGED (CTL_SCALE, frame_editor::OnScaleChangeFinal)
    EVT_TOOL (TOOL_SELECT, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_NEWBOX, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_DELETEBOX, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_RESIZE, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_TEST, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_MOVEUP, frame_editor::OnMoveUp)
    EVT_TOOL (TOOL_MOVEDOWN, frame_editor::OnMoveDown)
    EVT_LISTBOX (CTL_LIST_BOXES, frame_editor::OnSelectBox)
    EVT_LISTBOX_DCLICK (CTL_LIST_BOXES, frame_editor::EditSelectedBox)
    EVT_CLOSE (frame_editor::OnFrameClose)
END_EVENT_TABLE()

DECLARE_RESOURCE(icon_editor_png)
DECLARE_RESOURCE(tool_select_png)
DECLARE_RESOURCE(tool_newbox_png)
DECLARE_RESOURCE(tool_deletebox_png)
DECLARE_RESOURCE(tool_resize_png)
DECLARE_RESOURCE(tool_test_png)

constexpr size_t MAX_HISTORY_SIZE = 20;

frame_editor::frame_editor() : wxFrame(nullptr, wxID_ANY, "Layout Bolletta", wxDefaultPosition, wxSize(900, 700)) {
    m_config = new wxConfig("BillLayoutScript");

    wxMenuBar *menuBar = new wxMenuBar();
    
    m_bls_history = new wxFileHistory(MAX_RECENT_FILES_HISTORY, MENU_OPEN_RECENT);
    m_bls_history->UseMenu(m_bls_history_menu = new wxMenu);

    m_pdf_history = new wxFileHistory(MAX_RECENT_PDFS_HISTORY, MENU_OPEN_PDF_RECENT);
    m_pdf_history->UseMenu(m_pdf_history_menu = new wxMenu);

    m_config->SetPath("/RecentFiles");
    m_bls_history->Load(*m_config);

    m_config->SetPath("/RecentPdfs");
    m_pdf_history->Load(*m_config);

    m_config->SetPath("/");

    wxMenu *menuFile = new wxMenu;
    menuFile->Append(MENU_NEW, "&Nuovo\tCtrl-N", "Crea un Nuovo Layout");
    menuFile->Append(MENU_OPEN, "&Apri...\tCtrl-O", "Apri un Layout");
    menuFile->AppendSubMenu(m_bls_history_menu, "Apri &Recenti");
    menuFile->Append(MENU_SAVE, "&Salva\tCtrl-S", "Salva il Layout");
    menuFile->Append(MENU_SAVEAS, "Sa&lva con nome...\tCtrl-Shift-S", "Salva il Layout con nome...");
    menuFile->AppendSeparator();
    menuFile->Append(MENU_LOAD_PDF, "Carica &PDF\tCtrl-L", "Carica un file PDF");
    menuFile->AppendSubMenu(m_pdf_history_menu, "PDF Recenti...");
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
    menuEdit->AppendSeparator();
    menuEdit->Append(MENU_DELETE, "&Cancella Selezione\tDel", "Cancella il rettangolo selezionato");
    menuBar->Append(menuEdit, "&Modifica");

    wxMenu *menuLayout = new wxMenu;
    menuLayout->Append(MENU_EDITBOX, "Modifica &Rettangolo\tCtrl-E", "Modifica il rettangolo selezionato");
    menuLayout->Append(MENU_READDATA, "L&eggi Layout\tCtrl-R", "Test della lettura dei dati");
    menuLayout->Append(MENU_EDITCONTROL, "Modifica script di &controllo\tCtrl-L");
    menuLayout->Append(MENU_SETLANGUAGE, "Cambia lingua di layout");

    menuBar->Append(menuLayout, "&Layout");

    SetMenuBar(menuBar);

    wxToolBar *toolbar_top = CreateToolBar();

    toolbar_top->AddTool(MENU_NEW, "Nuovo", wxArtProvider::GetBitmap(wxART_NEW), "Nuovo");
    toolbar_top->AddTool(MENU_OPEN, "Apri", wxArtProvider::GetBitmap(wxART_FILE_OPEN), "Apri");
    toolbar_top->AddTool(MENU_SAVE, "Salva", wxArtProvider::GetBitmap(wxART_FILE_SAVE), "Salva");

    toolbar_top->AddSeparator();

    toolbar_top->AddTool(MENU_UNDO, "Annulla", wxArtProvider::GetBitmap(wxART_UNDO), "Annulla");
    toolbar_top->AddTool(MENU_REDO, "Ripeti", wxArtProvider::GetBitmap(wxART_REDO), "Ripeti");

    toolbar_top->AddSeparator();

    toolbar_top->AddTool(MENU_CUT, "Taglia", wxArtProvider::GetBitmap(wxART_CUT), "Taglia");
    toolbar_top->AddTool(MENU_COPY, "Copia", wxArtProvider::GetBitmap(wxART_COPY), "Copia");
    toolbar_top->AddTool(MENU_PASTE, "Incolla", wxArtProvider::GetBitmap(wxART_PASTE), "Incolla");

    toolbar_top->AddTool(MENU_READDATA, "Leggi Layout", wxArtProvider::GetBitmap(wxART_REPORT_VIEW), "Leggi Layout");

    toolbar_top->AddStretchableSpace();

    wxButton *btn_rotate = new wxButton(toolbar_top, CTL_ROTATE, "Ruota Immagine", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(btn_rotate, "Ruota in senso orario");

    wxButton *btn_load_pdf = new wxButton(toolbar_top, CTL_LOAD_PDF, "Carica PDF", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(btn_load_pdf, "Carica un file PDF");

    wxButton *btn_auto_layout = new wxButton(toolbar_top, CTL_AUTO_LAYOUT, "Auto carica layout", wxDefaultPosition, wxSize(150, -1));
    toolbar_top->AddControl(btn_auto_layout, "Determina il layout di questo file automaticamente");

    m_page = new wxSpinCtrl(toolbar_top, CTL_PAGE, "Pagina", wxDefaultPosition, wxSize(150, -1), wxTE_PROCESS_ENTER | wxSP_ARROW_KEYS, 0, 0);
    toolbar_top->AddControl(m_page, "Pagina");

    m_scale = new wxSlider(toolbar_top, CTL_SCALE, 50, 1, 100, wxDefaultPosition, wxSize(200, -1));
    toolbar_top->AddControl(m_scale, "Scala");

    toolbar_top->Realize();

    wxSplitterWindow *m_splitter = new wxSplitterWindow(this);

    wxPanel *m_panel_left = new wxPanel(m_splitter);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);

    wxToolBar *toolbar_side = new wxToolBar(m_panel_left, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_VERTICAL);

    auto loadPNG = [](const auto &resource) {
        return wxBitmap::NewFromPNGData(resource.data, resource.len);
    };

    toolbar_side->AddRadioTool(TOOL_SELECT, "Seleziona", loadPNG(GET_RESOURCE(tool_select_png)), wxNullBitmap, "Seleziona");
    toolbar_side->AddRadioTool(TOOL_NEWBOX, "Nuovo rettangolo", loadPNG(GET_RESOURCE(tool_newbox_png)), wxNullBitmap, "Nuovo rettangolo");
    toolbar_side->AddRadioTool(TOOL_DELETEBOX, "Cancella rettangolo", loadPNG(GET_RESOURCE(tool_deletebox_png)), wxNullBitmap, "Cancella rettangolo");
    toolbar_side->AddRadioTool(TOOL_RESIZE, "Ridimensiona rettangolo", loadPNG(GET_RESOURCE(tool_resize_png)), wxNullBitmap, "Ridimensiona rettangolo");
    toolbar_side->AddRadioTool(TOOL_TEST, "Test rettangolo", loadPNG(GET_RESOURCE(tool_test_png)), wxNullBitmap, "Test rettangolo");

    toolbar_side->AddSeparator();

    toolbar_side->AddTool(TOOL_MOVEUP, "Muovi su", wxArtProvider::GetBitmap(wxART_GO_UP), "Muovi su");
    toolbar_side->AddTool(TOOL_MOVEDOWN, L"Muovi giù", wxArtProvider::GetBitmap(wxART_GO_DOWN), L"Muovi giù");

    toolbar_side->Realize();
    sizer->Add(toolbar_side, 0, wxEXPAND);

    m_list_boxes = new wxListBox(m_panel_left, CTL_LIST_BOXES);
    sizer->Add(m_list_boxes, 1, wxEXPAND);

    m_panel_left->SetSizer(sizer);
    m_image = new box_editor_panel(m_splitter, this);

    m_splitter->SplitVertically(m_panel_left, m_image, 200);
    m_splitter->SetMinimumPaneSize(100);

    auto loadIcon = [&](const auto &resource) {
        wxIcon icon;
        icon.CopyFromBitmap(loadPNG(resource));
        return icon;
    };

    SetIcon(loadIcon(GET_RESOURCE(icon_editor_png)));
    Show();

    currentHistory = history.begin();
}

void frame_editor::openFile(const wxString &filename) {
    try {
        if (box_dialog::closeAll()) {
            m_filename = filename.ToStdString();
            layout = box_list::from_file(m_filename);

            modified = false;
            history.clear();
            updateLayout();

            m_bls_history->AddFileToHistory(filename);
            m_config->SetPath("/RecentFiles");
            m_bls_history->Save(*m_config);
            m_config->SetPath("/");
        }
    } catch (const layout_error &error) {
        wxMessageBox("Impossibile aprire questo file", "Errore", wxOK | wxICON_ERROR);
    }
}

bool frame_editor::save(bool saveAs) {
    if (m_filename.empty() || saveAs) {
        wxString lastLayoutDir = m_config->Read("LastLayoutDir");
        wxFileDialog diag(this, "Salva Layout Bolletta", lastLayoutDir, m_filename.string(), "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*", wxFD_SAVE);

        if (diag.ShowModal() == wxID_CANCEL)
            return false;

        m_config->Write("LastLayoutDir", wxFileName(diag.GetPath()).GetPath());
        m_filename = diag.GetPath().ToStdString();
    }
    try {
        layout.save_file(m_filename);
    } catch (const layout_error &error) {
        wxMessageBox(error.what(), "Errore", wxICON_ERROR);
        return false;
    }
    modified = false;
    return true;
}

bool frame_editor::saveIfModified() {
    if (modified) {
        wxMessageDialog dialog(this, "Salvare le modifiche?", "Layout Bolletta", wxYES_NO | wxCANCEL | wxICON_WARNING);

        switch (dialog.ShowModal()) {
        case wxID_YES:
            return save();
        case wxID_NO:
            return true;
        case wxID_CANCEL:
            return false;
        }
    }
    return true;
}

void frame_editor::updateLayout(bool addToHistory) {
    m_list_boxes->Clear();
    for (auto &box : layout) {
        if (box.name.empty()) {
            m_list_boxes->Append("(Senza nome)");
        } else {
            m_list_boxes->Append(box.name);
        }
    }
    m_image->Refresh();

    if (addToHistory) {
        if (!history.empty()) {
            modified = true;
        }
        while (!history.empty() && history.end() > currentHistory + 1) {
            history.pop_back();
        }
        history.push_back(layout);
        if (history.size() > MAX_HISTORY_SIZE) {
            history.pop_front();
        }
        currentHistory = history.end() - 1;
    }
}

void frame_editor::loadPdf(const wxString &filename) {
    try {
        m_doc.open(filename.ToStdString());
        m_page->SetRange(1, m_doc.num_pages());
        setSelectedPage(1, true);

        m_pdf_history->AddFileToHistory(m_doc.filename().string());

        m_config->SetPath("/RecentPdfs");
        m_pdf_history->Save(*m_config);
        m_config->SetPath("/");
    } catch (const pdf_error &error) {
        wxMessageBox(error.what(), "Errore", wxICON_ERROR);
    }
}

wxString frame_editor::getControlScript(bool open_dialog) {
    wxString filename = m_config->Read("ControlScriptFilename");
    if (filename.empty() || open_dialog) {
        wxFileDialog diag(this, "Apri script di controllo", wxFileName(filename).GetPath(), wxEmptyString, "File bls (*.bls)|*.bls|Tutti i file (*.*)|*.*");

        if (diag.ShowModal() == wxID_OK) {
            filename = diag.GetPath();
            m_config->Write("ControlScriptFilename", filename);
        }
    }
    return filename;
}

void frame_editor::setSelectedPage(int page, bool force) {
    if (!force && page == selected_page) return;
    if (!m_doc.isopen()) return;

    if (page > m_doc.num_pages() || page <= 0) {
        wxBell();
        return;
    }
    
    selected_page = page;

    m_page->SetValue(wxString::Format("%i/%i", page, m_page->GetMax()));
    m_image->setImage(pdf_to_image(m_doc, page, rotation));
}

void frame_editor::selectBox(layout_box *box) {
    m_image->setSelectedBox(box);
    if (box) {
        m_list_boxes->SetSelection(std::distance(layout.begin(),
            std::ranges::find(layout, box,
            [](const auto &box) { return &box; })));
    } else {
        m_list_boxes->SetSelection(-1);
    }
    m_image->Refresh();
}