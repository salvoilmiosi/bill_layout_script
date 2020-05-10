#include "editor.h"

#include <fstream>
#include <sstream>

#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/artprov.h>
#include <wx/splitter.h>

#include "box_editor_panel.h"
#include "pdf_to_image.h"
#include "box_dialog.h"
#include "resources.h"

BEGIN_EVENT_TABLE(frame_editor, wxFrame)
    EVT_MENU (MENU_NEW, OnNewFile)
    EVT_MENU (MENU_OPEN, OnOpenFile)
    EVT_MENU (MENU_SAVE, OnSaveFile)
    EVT_MENU (MENU_SAVEAS, OnSaveFileAs)
    EVT_MENU (MENU_CLOSE, OnMenuClose)
    EVT_MENU (MENU_UNDO, OnUndo)
    EVT_MENU (MENU_REDO, OnRedo)
    EVT_MENU (MENU_CUT, OnCut)
    EVT_MENU (MENU_COPY, OnCopy)
    EVT_MENU (MENU_PASTE, OnPaste)
    EVT_MENU (MENU_LOAD_PDF, OnLoadPdf)
    EVT_MENU (MENU_EDITBOX, EditSelectedBox)
    EVT_MENU (MENU_DELETE, OnDelete)
    EVT_MENU (MENU_READDATA, OnReadData)
    EVT_BUTTON (CTL_LOAD_PDF, OnLoadPdf)
    EVT_COMBOBOX (CTL_PAGE, OnPageSelect)
    EVT_TEXT_ENTER (CTL_PAGE, OnPageSelect)
    EVT_SLIDER (CTL_SCALE, OnScaleChange)
    EVT_TOOL (TOOL_SELECT, OnChangeTool)
    EVT_TOOL (TOOL_NEWBOX, OnChangeTool)
    EVT_TOOL (TOOL_DELETEBOX, OnChangeTool)
    EVT_TOOL (TOOL_MOVEUP, OnMoveUp)
    EVT_TOOL (TOOL_MOVEDOWN, OnMoveDown)
    EVT_LISTBOX (CTL_LIST_BOXES, OnSelectBox)
    EVT_LISTBOX_DCLICK (CTL_LIST_BOXES, EditSelectedBox)
    EVT_CLOSE (OnFrameClose)
END_EVENT_TABLE()

DECLARE_RESOURCE(tool_select_png)
DECLARE_RESOURCE(tool_newbox_png)
DECLARE_RESOURCE(tool_deletebox_png)
DECLARE_RESOURCE(icon_editor_png)

const std::string &get_app_path() {
    static wxFileName f{wxStandardPaths::Get().GetExecutablePath()};
    static std::string path{f.GetPath().ToStdString()};
    return path;
}

frame_editor::frame_editor() : wxFrame(nullptr, wxID_ANY, "Layout Bolletta", wxDefaultPosition, wxSize(900, 700)) {
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

    wxMenu *menuLayout = new wxMenu;
    menuLayout->Append(MENU_LOAD_PDF, "Carica &PDF\tCtrl-L", "Carica un file PDF");
    menuLayout->Append(MENU_EDITBOX, "Modifica &Opzioni\tCtrl-E", "Modifica il rettangolo selezionato");
    menuLayout->Append(MENU_DELETE, "&Cancella Selezione\tDel", "Cancella il rettangolo selezionato");
    menuLayout->Append(MENU_READDATA, "L&eggi Layout\tCtrl-R", "Test della lettura dei dati");

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

    toolbar_top->AddSeparator();

    toolbar_top->AddStretchableSpace();

    wxButton *btn_load_pdf = new wxButton(toolbar_top, CTL_LOAD_PDF, "Carica PDF", wxDefaultPosition, wxSize(100, -1));
    toolbar_top->AddControl(btn_load_pdf, "Carica un file PDF");

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

void frame_editor::openFile(const std::string &filename) {
    try {
        layout_filename = filename;
        std::ifstream ifs(layout_filename);
        layout.clear();
        ifs >> layout;
        ifs.close();
        history.clear();
        loadPdf(layout.pdf_filename);
        updateLayout();
    } catch (layout_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void frame_editor::OnOpenFile(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri Layout Bolletta", wxEmptyString, wxEmptyString, "File layout (*.layout)|*.layout|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    if (saveIfModified()) {
        modified = false;
        openFile(diag.GetPath().ToStdString());
    }
}

bool frame_editor::save(bool saveAs) {
    if (layout_filename.empty() || saveAs) {
        wxFileDialog diag(this, "Salva Layout Bolletta", wxEmptyString, layout_filename, "File layout (*.layout)|*.layout|Tutti i file (*.*)|*.*", wxFD_SAVE);

        if (diag.ShowModal() == wxID_CANCEL)
            return false;

        layout_filename = diag.GetPath().ToStdString();
    }
    try {
        std::ofstream ofs(layout_filename);
        ofs << layout;
        ofs.close();
        modified = false;
    } catch (layout_error &error) {
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
    m_image->paintNow();

    if (!history.empty()) {
        modified = true;
    }

    if (addToHistory) {
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
    if (selection >= 0) {
        clipboard = std::make_unique<layout_box>(layout.boxes[selection]);
        layout.boxes.erase(layout.boxes.begin() + selection);
        updateLayout();
    }
}

void frame_editor::OnCopy(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        clipboard = std::make_unique<layout_box>(layout.boxes[selection]);
        updateLayout();
    }
}

void frame_editor::OnPaste(wxCommandEvent &evt) {
    if (clipboard) {
        if (clipboard->page != selected_page) {
            clipboard->page = selected_page;
        } else {
            clipboard->x += 10 / m_image->getScaledWidth();
            clipboard->y += 10 / m_image->getScaledHeight();
        }
        layout.boxes.push_back(*clipboard);
        updateLayout();
        selectBox(layout.boxes.size() - 1);
        clipboard = nullptr;
    }
}

void frame_editor::loadPdf(const std::string &pdf_filename) {
    layout.pdf_filename = pdf_filename;
    info = xpdf::pdf_get_info(get_app_path(), layout.pdf_filename);
    m_page->Clear();
    for (int i=1; i<=info.num_pages; ++i) {
        m_page->Append(wxString::Format("%i", i));
    }
    setSelectedPage(1, true);
}

void frame_editor::OnLoadPdf(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    loadPdf(diag.GetPath().ToStdString());
    updateLayout();
}

void frame_editor::setSelectedPage(int page, bool force) {
    if (!force && page == selected_page) return;
    if (layout.pdf_filename.empty()) return;

    m_page->SetSelection(page - 1);
    try {
        selected_page = page;
        m_image->setImage(xpdf::pdf_to_image(get_app_path(), layout.pdf_filename, page));
    } catch (const pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxOK | wxICON_ERROR);
    }
}

void frame_editor::OnPageSelect(wxCommandEvent &evt) {
    if (layout.pdf_filename.empty()) return;

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

void frame_editor::OnScaleChange(wxCommandEvent &evt) {
    m_image->rescale(m_scale->GetValue() / 100.f);
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
    }
    m_image->paintNow();
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
    if (layout.pdf_filename.empty()) return;

    char cmd_str[FILENAME_MAX];
    snprintf(cmd_str, FILENAME_MAX, "%s/layout_reader", get_app_path().c_str());

    char pdf_str[FILENAME_MAX];
    snprintf(pdf_str, FILENAME_MAX, "%s", layout.pdf_filename.c_str());

    const char *args[] = {
        cmd_str,
        pdf_str, "-",
        nullptr
    };

    try {
        auto pipe = open_process(args);
        std::ostringstream oss;
        oss << layout;
        pipe->write_all(oss.str());
        pipe->close_stdin();
        std::string output = pipe->read_all();
        wxMessageBox(output, "Output di layout_reader", wxICON_INFORMATION);
    } catch (pipe_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
    }
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