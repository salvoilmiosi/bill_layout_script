#include "editor.h"

#include <fstream>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/splitter.h>
#include <wx/config.h>

#include "resources.h"
#include "box_editor_panel.h"
#include "pdf_to_image.h"

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
    EVT_BUTTON(CTL_AUTO_LAYOUT, frame_editor::OnAutoLayout)
    EVT_BUTTON (CTL_LOAD_PDF, frame_editor::OnLoadPdf)
    EVT_COMBOBOX (CTL_PAGE, frame_editor::OnPageSelect)
    EVT_TEXT_ENTER (CTL_PAGE, frame_editor::OnPageSelect)
    EVT_COMMAND_SCROLL_THUMBTRACK (CTL_SCALE, frame_editor::OnScaleChange)
    EVT_COMMAND_SCROLL_CHANGED (CTL_SCALE, frame_editor::OnScaleChangeFinal)
    EVT_TOOL (TOOL_SELECT, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_NEWBOX, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_DELETEBOX, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_RESIZE, frame_editor::OnChangeTool)
    EVT_TOOL (TOOL_MOVEUP, frame_editor::OnMoveUp)
    EVT_TOOL (TOOL_MOVEDOWN, frame_editor::OnMoveDown)
    EVT_LISTBOX (CTL_LIST_BOXES, frame_editor::OnSelectBox)
    EVT_LISTBOX_DCLICK (CTL_LIST_BOXES, frame_editor::EditSelectedBox)
    EVT_CLOSE (frame_editor::OnFrameClose)
END_EVENT_TABLE()

DECLARE_RESOURCE(tool_select_png)
DECLARE_RESOURCE(tool_newbox_png)
DECLARE_RESOURCE(tool_deletebox_png)
DECLARE_RESOURCE(tool_resize_png)
DECLARE_RESOURCE(icon_editor_png)

constexpr size_t MAX_HISTORY_SIZE = 20;
constexpr size_t MAX_RECENT_FILES_HISTORY = 10;
constexpr size_t MAX_RECENT_PDFS_HISTORY = 10;

frame_editor::frame_editor() : wxFrame(nullptr, wxID_ANY, "Layout Bolletta", wxDefaultPosition, wxSize(900, 700)) {
    wxMenuBar *menuBar = new wxMenuBar();
    
    m_recent = new wxMenu;
    m_recent_pdfs = new wxMenu;
    updateRecentFiles();

    wxMenu *menuFile = new wxMenu;
    menuFile->Append(MENU_NEW, "&Nuovo\tCtrl-N", "Crea un Nuovo Layout");
    menuFile->Append(MENU_OPEN, "&Apri...\tCtrl-O", "Apri un Layout");
    menuFile->AppendSubMenu(m_recent, "Apri &Recenti");
    menuFile->Append(MENU_SAVE, "&Salva\tCtrl-S", "Salva il Layout");
    menuFile->Append(MENU_SAVEAS, "Sa&lva con nome...\tCtrl-Shift-S", "Salva il Layout con nome...");
    menuFile->AppendSeparator();
    menuFile->Append(MENU_LOAD_PDF, "Carica &PDF\tCtrl-L", "Carica un file PDF");
    menuFile->AppendSubMenu(m_recent_pdfs, "PDF Recenti...");
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
    menuLayout->Append(MENU_LAYOUTPATH, "Modifica cartella layout\tCtrl-K");

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

    wxButton *btn_load_pdf = new wxButton(toolbar_top, CTL_LOAD_PDF, "Carica PDF", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(btn_load_pdf, "Carica un file PDF");

    wxButton *btn_auto_layout = new wxButton(toolbar_top, CTL_AUTO_LAYOUT, "Auto carica layout", wxDefaultPosition, wxSize(150, -1));
    toolbar_top->AddControl(btn_auto_layout, "Determina il layout di questo file automaticamente");

    m_page = new wxComboBox(toolbar_top, CTL_PAGE, "Pagina", wxDefaultPosition, wxSize(100, -1), 0, nullptr, wxTE_PROCESS_ENTER);
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
        layout = bill_layout_script::from_file(filename.ToStdString());
    } catch (const layout_error &error) {
        wxMessageBox("Impossibile aprire questo file", "Errore", wxOK | wxICON_ERROR);
        return;
    }
    modified = false;
    history.clear();
    updateLayout();
    
    auto it = std::find(recentFiles.begin(), recentFiles.end(), filename);
    if (it != recentFiles.end()) {
        recentFiles.erase(it);
    }
    recentFiles.push_front(filename);
    if (recentFiles.size() > MAX_RECENT_FILES_HISTORY) {
        recentFiles.pop_back();
    }
    updateRecentFiles(true);
}

bool frame_editor::save(bool saveAs) {
    if (layout.m_filename.empty() || saveAs) {
        wxFileDialog diag(this, "Salva Layout Bolletta", wxEmptyString, layout.m_filename.string(), "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*", wxFD_SAVE);

        if (diag.ShowModal() == wxID_CANCEL)
            return false;

        layout.m_filename = diag.GetPath().ToStdString();
    }
    if (!layout.save_file(layout.m_filename)) {
        wxMessageBox("Impossibile salvare questo file", "Errore", wxICON_ERROR);
        return false;
    }
    modified = false;
    return true;
}

bool frame_editor::saveIfModified() {
    if (modified) {
        wxMessageDialog dialog(this, "Salvare le modifiche?", "Layout Bolletta", wxYES_NO | wxCANCEL);

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
    for (size_t i=0; i<layout.m_boxes.size(); ++i) {
        auto &box = layout.m_boxes[i];
        m_list_boxes->Append(box->name);
        if (box->selected) {
            m_list_boxes->SetSelection(i);
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
        history.push_back(copyLayout(layout));
        if (history.size() > MAX_HISTORY_SIZE) {
            history.pop_front();
        }
        currentHistory = history.end() - 1;
    }
}

void frame_editor::updateRecentFiles(bool save) {
    if (save) {
        wxConfig::Get()->DeleteGroup("RecentFiles");
        wxConfig::Get()->SetPath("/RecentFiles");
        for (size_t i=0; i<recentFiles.size(); ++i) {
            wxConfig::Get()->Write(wxString::Format("%lld", i), recentFiles[i]);
        }
        wxConfig::Get()->DeleteGroup("RecentPdfs");
        wxConfig::Get()->SetPath("/RecentPdfs");
        for (size_t i=0; i<recentPdfs.size(); ++i) {
            wxConfig::Get()->Write(wxString::Format("%lld", i), recentPdfs[i]);
        }
        wxConfig::Get()->SetPath("/");
    } else {
        recentFiles.clear();
        wxConfig::Get()->SetPath("/RecentFiles");
        size_t len = wxConfig::Get()->GetNumberOfEntries();
        for (size_t i=0; i<len; ++i) {
            recentFiles.push_back(wxConfig::Get()->Read(wxString::Format("%lld", i)));
        }
        recentPdfs.clear();
        wxConfig::Get()->SetPath("/RecentPdfs");
        len = wxConfig::Get()->GetNumberOfEntries();
        for (size_t i=0; i<len; ++i) {
            recentPdfs.push_back(wxConfig::Get()->Read(wxString::Format("%lld", i)));
        }
        wxConfig::Get()->SetPath("/");
    }
    size_t len = m_recent->GetMenuItemCount();
    for (size_t i=0; i<len; ++i) {
        m_recent->Delete(MENU_OPEN_RECENT + i);
    }
    for (size_t i=0; i<recentFiles.size(); ++i) {
        m_recent->Append(MENU_OPEN_RECENT + i, recentFiles[i]);
    }
    len = m_recent_pdfs->GetMenuItemCount();
    for (size_t i=0; i<len; ++i) {
        m_recent_pdfs->Delete(MENU_OPEN_PDF_RECENT + i);
    }
    for (size_t i=0; i<recentPdfs.size(); ++i) {
        m_recent_pdfs->Append(MENU_OPEN_PDF_RECENT + i, recentPdfs[i]);
    }
}

void frame_editor::loadPdf(const wxString &filename) {
    try {
        m_doc.open(filename.ToStdString());
        m_page->Clear();
        for (int i=1; i<=m_doc.num_pages(); ++i) {
            m_page->Append(wxString::Format("%i", i));
        }
        setSelectedPage(1, true);
        
        auto it = std::find(recentPdfs.begin(), recentPdfs.end(), wxString(m_doc.filename()));
        if (it != recentPdfs.end()) {
            recentPdfs.erase(it);
        }
        recentPdfs.push_front(m_doc.filename());
        if (recentPdfs.size() > MAX_RECENT_PDFS_HISTORY) {
            recentPdfs.pop_back();
        }
        updateRecentFiles(true);
    } catch (const pdf_error &error) {
        wxMessageBox(error.what(), "Errore", wxICON_ERROR);
    }
}

wxString frame_editor::getControlScript(bool open_dialog) {
    wxString filename = wxConfig::Get()->Read("ControlScriptFilename");
    if (filename.empty() || open_dialog) {
        wxFileDialog diag(this, "Apri script di controllo", wxEmptyString, wxEmptyString, "File bls (*.bls)|*.bls|Tutti i file (*.*)|*.*");

        if (diag.ShowModal() == wxID_OK) {
            filename = diag.GetPath();
            wxConfig::Get()->Write("ControlScriptFilename", filename);
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

    m_page->SetSelection(page - 1);
    m_image->setImage(pdf_to_image(m_doc, page));
}

void frame_editor::selectBox(const box_ptr &box) {
    for (size_t i=0; i<layout.m_boxes.size(); ++i) {
        if (layout.m_boxes[i] == box) {
            m_list_boxes->SetSelection(i);
            layout.m_boxes[i]->selected = true;
        } else {
            layout.m_boxes[i]->selected = false;
        }
    }
    if (box) {
        setSelectedPage(box->page);
        m_image->setSelectedBox(box);
    } else {
        m_list_boxes->SetSelection(-1);
    }
    m_image->Refresh();
}