#include "editor.h"

#include <wx/filename.h>
#include <wx/config.h>

#include <fstream>

#include "utils.h"

#include "layout_ext.h"
#include "clipboard.h"
#include "box_editor_panel.h"
#include "box_dialog.h"
#include "output_dialog.h"

#include "parser.h"
#include "reader.h"

void frame_editor::OnNewFile(wxCommandEvent &evt) {
    if (saveIfModified()) {
        modified = false;
        layout.clear();
        history.clear();
        updateLayout(false);
    }
}

void frame_editor::OnOpenFile(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri Layout Bolletta", wxEmptyString, wxEmptyString, "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    if (saveIfModified()) {
        openFile(diag.GetPath().ToStdString());
    }
}

void frame_editor::OnOpenRecent(wxCommandEvent &evt) {
    if (saveIfModified()) {
        openFile(recentFiles[evt.GetId() - MENU_OPEN_RECENT]);
    }
}

void frame_editor::OnOpenRecentPdf(wxCommandEvent &evt) {
    loadPdf(recentPdfs[evt.GetId() - MENU_OPEN_PDF_RECENT]);
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

void frame_editor::OnUndo(wxCommandEvent &evt) {
    if (currentHistory > history.begin()) {
        --currentHistory;
        layout = copyLayout(*currentHistory);
        updateLayout(false);
    } else {
        wxBell();
    }
}

void frame_editor::OnRedo(wxCommandEvent &evt) {
    if (currentHistory < history.end() - 1) {
        ++currentHistory;
        layout = copyLayout(*currentHistory);
        updateLayout(false);
    } else {
        wxBell();
    }
}

void frame_editor::OnCut(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && SetClipboard(*layout.m_boxes[selection])) {
        layout.m_boxes.erase(layout.m_boxes.begin() + selection);
        updateLayout();
    }
}

void frame_editor::OnCopy(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        SetClipboard(*layout.m_boxes[selection]);
    }
}

void frame_editor::OnPaste(wxCommandEvent &evt) {
    layout_box clipboard;
    if (!GetClipboard(clipboard)) return;
    
    if (clipboard.page != selected_page) {
        clipboard.page = selected_page;
    }
    
    auto &box = insertAfterSelected(layout, std::move(clipboard));
    updateLayout();
    selectBox(box);
}

void frame_editor::OpenControlScript(wxCommandEvent &evt) {
    getControlScript(true);
}

void frame_editor::OnAutoLayout(wxCommandEvent &evt) {
    if (!m_doc.isopen()) {
        wxBell();
        return;
    }

    try {
        reader my_reader(m_doc, bill_layout_script::from_file(getControlScript().ToStdString()));
        my_reader.start();

        auto &layouts = my_reader.get_output().layouts;
        if (layouts.size() <= 1) {
            wxMessageBox("Impossibile determinare il layout di questo file", "Errore", wxOK | wxICON_WARNING);
        } else if (saveIfModified()) {
            openFile(layouts.back());
        }
    } catch (const std::exception &error) {
        wxMessageBox(error.what(), "Errore", wxICON_ERROR);
    }
}

void frame_editor::OnLoadPdf(wxCommandEvent &evt) {
    wxFileDialog diag(this, "Apri PDF", wxEmptyString, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    loadPdf(diag.GetPath().ToStdString());
    updateLayout(false);
}

void frame_editor::OnPageSelect(wxCommandEvent &evt) {
    if (!m_doc.isopen()) return;

    long selected_page;
    if (m_page->GetValue().ToLong(&selected_page)) {
        setSelectedPage(selected_page);
    } else {
        wxBell();
    }
}

void frame_editor::OnChangeTool(wxCommandEvent &evt) {
    m_image->setSelectedTool(evt.GetId());
}

void frame_editor::OnSelectBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.m_boxes.size()) {
        selectBox(layout.m_boxes[selection]);
    } else {
        selectBox(nullptr);
    }
}

void frame_editor::EditSelectedBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.m_boxes.size()) {
        new box_dialog(this, *layout.m_boxes[selection]);
    }
}

void frame_editor::OnDelete(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.m_boxes.size()) {
        layout.m_boxes.erase(layout.m_boxes.begin() + selection);
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
        std::swap(layout.m_boxes[selection], layout.m_boxes[selection-1]);
        updateLayout();
    }
}

void frame_editor::OnMoveDown(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int)layout.m_boxes.size() - 1) {
        std::swap(layout.m_boxes[selection], layout.m_boxes[selection+1]);
        updateLayout();
    }
}

void frame_editor::OnScaleChange(wxScrollEvent &evt) {
    m_image->rescale(m_scale->GetValue() / 100.f);
}

void frame_editor::OnScaleChangeFinal(wxScrollEvent &evt) {
    m_image->rescale(m_scale->GetValue() / 100.f, wxIMAGE_QUALITY_HIGH);
}

void frame_editor::OnFrameClose(wxCloseEvent &evt) {
    if (saveIfModified()) {
        evt.Skip();
    }
}