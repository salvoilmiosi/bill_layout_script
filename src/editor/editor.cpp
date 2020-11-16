#include "editor.h"

#include <fstream>
#include <sstream>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/splitter.h>
#include <wx/config.h>

#include "box_editor_panel.h"
#include "pdf_to_image.h"
#include "box_dialog.h"
#include "output_dialog.h"
#include "resources.h"
#include "clipboard.h"

#include "../shared/pipe.h"

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

const std::string &get_app_path() {
    static wxFileName f{wxStandardPaths::Get().GetExecutablePath()};
    static std::string path{f.GetPath().ToStdString()};
    return path;
}

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
    menuLayout->Append(MENU_EDITBOX, "Modifica &Opzioni\tEnter", "Modifica il rettangolo selezionato");
    menuLayout->Append(MENU_READDATA, "L&eggi Layout\tCtrl-R", "Test della lettura dei dati");
    menuLayout->Append(MENU_EDITCONTROL, "Modifica script di &controllo\tCtrl-L");

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

    m_page = new wxComboBox(toolbar_top, CTL_PAGE, "Pagina", wxDefaultPosition, wxSize(100, -1));
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

void frame_editor::OnNewFile(wxCommandEvent &evt) {
    if (saveIfModified()) {
        modified = false;
        layout_filename.clear();
        layout.clear();
        history.clear();
        updateLayout();
    }
}

void frame_editor::openFile(const wxString &filename) {
    try {
        layout_filename = filename;
        std::ifstream ifs(layout_filename.ToStdString());
        layout.clear();
        ifs >> layout;
        ifs.close();
        history.clear();
        if (wxFileExists(pdf_filename)) {
            loadPdf(pdf_filename);
        }
        updateLayout();
        
        static constexpr size_t MAX_RECENT_FILES_HISTORY = 10;
        if (auto it = std::find(recentFiles.begin(), recentFiles.end(), layout_filename); it != recentFiles.end()) {
            recentFiles.erase(it);
        }
        recentFiles.push_front(layout_filename);
        if (recentFiles.size() > MAX_RECENT_FILES_HISTORY) {
            recentFiles.pop_back();
        }
        updateRecentFiles(true);
    } catch (const layout_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void frame_editor::OnOpenFile(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri Layout Bolletta", wxEmptyString, wxEmptyString, "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    if (saveIfModified()) {
        modified = false;
        openFile(diag.GetPath().ToStdString());
    }
}

void frame_editor::OnOpenRecent(wxCommandEvent &evt) {
    if (saveIfModified()) {
        modified = false;
        openFile(recentFiles[evt.GetId() - MENU_OPEN_RECENT]);
    }
}

void frame_editor::OnOpenRecentPdf(wxCommandEvent &evt) {
    loadPdf(recentPdfs[evt.GetId() - MENU_OPEN_PDF_RECENT]);
}

bool frame_editor::save(bool saveAs) {
    if (layout_filename.empty() || saveAs) {
        wxFileDialog diag(this, "Salva Layout Bolletta", wxEmptyString, layout_filename, "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*", wxFD_SAVE);

        if (diag.ShowModal() == wxID_CANCEL)
            return false;

        layout_filename = diag.GetPath().ToStdString();
    }
    try {
        std::ofstream ofs(layout_filename.ToStdString());
        ofs << layout;
        ofs.close();
        modified = false;
    } catch (const layout_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
        return false;
    }
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

void frame_editor::OnSaveFile(wxCommandEvent &evt) {
    save();
}

void frame_editor::OnSaveFileAs(wxCommandEvent &evt) {
    save(true);
}

void frame_editor::OnMenuClose(wxCommandEvent &evt) {
    Close();
}

static constexpr size_t MAX_HISTORY_SIZE = 20;

void frame_editor::updateLayout(bool addToHistory) {
    m_list_boxes->Clear();
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        auto &box = layout.boxes[i];
        m_list_boxes->Append(box.name);
        if (box.selected) {
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
        history.push_back(layout);
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

void frame_editor::OnUndo(wxCommandEvent &evt) {
    if (currentHistory > history.begin()) {
        --currentHistory;
        layout = *currentHistory;
        updateLayout(false);
    } else {
        wxBell();
    }
}

void frame_editor::OnRedo(wxCommandEvent &evt) {
    if (currentHistory < history.end() - 1) {
        ++currentHistory;
        layout = *currentHistory;
        updateLayout(false);
    } else {
        wxBell();
    }
}

void frame_editor::OnCut(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && SetClipboard(layout.boxes[selection])) {
        layout.boxes.erase(layout.boxes.begin() + selection);
        updateLayout();
    }
}

void frame_editor::OnCopy(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        SetClipboard(layout.boxes[selection]);
    }
}

void frame_editor::OnPaste(wxCommandEvent &evt) {
    if (layout_box clipboard; GetClipboard(clipboard)) {
        if (clipboard.page != selected_page) {
            clipboard.page = selected_page;
        }
        
        layout.boxes.push_back(clipboard);
        updateLayout();
        selectBox(layout.boxes.size() - 1);
    }
}

void frame_editor::loadPdf(const wxString &filename) {
    try {
        info = pdf_get_info(filename.ToStdString());
        pdf_filename = filename;
        m_page->Clear();
        for (int i=1; i<=info.num_pages; ++i) {
            m_page->Append(wxString::Format("%i", i));
        }
        setSelectedPage(1, true);
        
        static constexpr size_t MAX_RECENT_PDFS_HISTORY = 10;
        if (auto it = std::find(recentPdfs.begin(), recentPdfs.end(), pdf_filename); it != recentPdfs.end()) {
            recentPdfs.erase(it);
        }
        recentPdfs.push_front(pdf_filename);
        if (recentPdfs.size() > MAX_RECENT_PDFS_HISTORY) {
            recentPdfs.pop_back();
        }
        updateRecentFiles(true);
    } catch (const xpdf_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
    }
}

wxString frame_editor::getControlScript() {
    wxFileDialog diag(this, "Apri script di controllo", wxEmptyString, wxEmptyString, "File layout compilati (*.out)|*.out|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return wxString();
    
    wxString filename = diag.GetPath().ToStdString();
    wxConfig::Get()->Write("ControlScriptFilename", filename);
    return filename;
}

void frame_editor::OpenControlScript(wxCommandEvent &evt) {
    getControlScript();
}

void frame_editor::OnAutoLayout(wxCommandEvent &evt) {
    if (pdf_filename.empty()) {
        wxBell();
        return;
    }

    wxString control_script_filename = wxConfig::Get()->Read("ControlScriptFilename");
    if (control_script_filename.empty()) {
        control_script_filename = getControlScript();
    }
    
    wxString cmd_str = get_app_path() + "/layout_reader";
    wxString layout_path = control_script_filename.substr(0, control_script_filename.find_last_of("\\/"));

    const char *args[] = {
        cmd_str.c_str(),
        "-p", pdf_filename.c_str(),
        control_script_filename.c_str(),
        nullptr
    };

    std::istringstream iss(open_process(args)->read_all());

    Json::Value json_output;
    try {
        iss >> json_output;

        std::string output_layout = json_output["globals"]["layout"].asString() + ".bls";
        if (output_layout.empty()) {
            wxMessageBox("Impossibile determinare il layout di questo file", "Errore", wxOK | wxICON_WARNING);
        } else if (saveIfModified()) {
            modified = false;
            openFile(layout_path + wxFileName::GetPathSeparator() + output_layout);
        }
    } catch (const std::exception &error) {
        wxMessageBox("Impossibile leggere l'output: " + iss.str(), "Errore", wxOK | wxICON_ERROR);
    }
}

void frame_editor::OnLoadPdf(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    loadPdf(diag.GetPath().ToStdString());
    updateLayout(false);
}

void frame_editor::setSelectedPage(int page, bool force) {
    if (!force && page == selected_page) return;
    if (pdf_filename.empty()) return;

    m_page->SetSelection(page - 1);
    try {
        selected_page = page;
        m_image->setImage(pdf_to_image(pdf_filename.ToStdString(), page));
    } catch (const xpdf_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void frame_editor::OnPageSelect(wxCommandEvent &evt) {
    if (pdf_filename.empty()) return;

    try {
        int page = std::stoi(m_page->GetValue().ToStdString());
        if (page > info.num_pages || page <= 0) {
            wxBell();
        } else {
            setSelectedPage(page);
        }
    } catch (const std::invalid_argument &error) {
        wxBell();
    }
}

void frame_editor::OnScaleChange(wxScrollEvent &evt) {
    m_image->rescale(m_scale->GetValue() / 100.f);
}

void frame_editor::OnScaleChangeFinal(wxScrollEvent &evt) {
    m_image->rescale(m_scale->GetValue() / 100.f, wxIMAGE_QUALITY_HIGH);
}

void frame_editor::OnChangeTool(wxCommandEvent &evt) {
    m_image->setSelectedTool(evt.GetId());
}

void frame_editor::OnSelectBox(wxCommandEvent &evt) {
    size_t selection = m_list_boxes->GetSelection();
    selectBox(selection);
}

void frame_editor::selectBox(int selection) {
    m_list_boxes->SetSelection(selection);
    for (size_t i=0; i<layout.boxes.size(); ++i) {
        layout.boxes[i].selected = (int)i == selection;
    }
    if (selection >= 0 && selection < (int) layout.boxes.size()) {
        setSelectedPage(layout.boxes[selection].page);
        m_image->setSelectedBox(layout.boxes.begin() + selection);
    }
    m_image->Refresh();
}

void frame_editor::EditSelectedBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        auto &box = layout.boxes[selection];
        box_dialog diag(this, box);

        if (diag.ShowModal() == wxID_OK) {
            updateLayout();
        }
    }
}

void frame_editor::OnDelete(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        layout.boxes.erase(layout.boxes.begin() + selection);
        updateLayout();
    }
}

void frame_editor::OnReadData(wxCommandEvent &evt) {
    static output_dialog *diag = new output_dialog(this);
    diag->compileAndRead();
    diag->Show();
}

void frame_editor::OnMoveUp(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection > 0) {
        std::swap(layout.boxes[selection], layout.boxes[selection-1]);
        updateLayout();
    }
}

void frame_editor::OnMoveDown(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int)layout.boxes.size() - 1) {
        std::swap(layout.boxes[selection], layout.boxes[selection+1]);
        updateLayout();
    }
}

void frame_editor::OnFrameClose(wxCloseEvent &evt) {
    if (saveIfModified()) {
        evt.Skip();
    }
}