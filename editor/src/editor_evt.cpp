#include "editor.h"

#include <wx/filename.h>
#include <wx/config.h>

#include <fstream>

#include "utils.h"

#include "clipboard.h"
#include "box_editor_panel.h"
#include "box_dialog.h"
#include "output_dialog.h"
#include "reader.h"
#include "layout_options_dialog.h"

void frame_editor::OnNewFile(wxCommandEvent &evt) {
    if (box_dialog::closeAll() && saveIfModified()) {
        modified = false;
        m_filename.clear();
        layout.clear();
        history.clear();
        updateLayout(false);
    }
}

void frame_editor::OnOpenFile(wxCommandEvent &evt) {
    wxString lastLayoutDir = m_config->Read("LastLayoutDir");
    wxFileDialog diag(this, "Apri Layout Bolletta", lastLayoutDir, wxEmptyString, "File layout (*.bls)|*.bls|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    if (saveIfModified()) {
        m_config->Write("LastLayoutDir", wxFileName(diag.GetPath()).GetPath());
        openFile(diag.GetPath().ToStdString());
    }
}

void frame_editor::OnOpenRecent(wxCommandEvent &evt) {
    if (saveIfModified()) {
        openFile(m_bls_history->GetHistoryFile(evt.GetId() - m_bls_history->GetBaseId()));
    }
}

void frame_editor::OnOpenRecentPdf(wxCommandEvent &evt) {
    loadPdf(m_pdf_history->GetHistoryFile(evt.GetId() - m_pdf_history->GetBaseId()));
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
        if (box_dialog::closeAll()) {
            --currentHistory;
            layout = *currentHistory;
            updateLayout(false);
        }
    } else {
        wxBell();
    }
}

void frame_editor::OnRedo(wxCommandEvent &evt) {
    if (currentHistory < history.end() - 1) {
        if (box_dialog::closeAll()) {
            ++currentHistory;
            layout = *currentHistory;
            updateLayout(false);
        }
    } else {
        wxBell();
    }
}

void frame_editor::OnCut(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        auto it = std::next(layout.begin(), selection);
        if (SetClipboard(*it) && box_dialog::closeDialog(*it)) {
            layout.erase(it);
            updateLayout();
        }
    }
}

void frame_editor::OnCopy(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        auto it = std::next(layout.begin(), selection);
        SetClipboard(*it);
    }
}

void frame_editor::OnPaste(wxCommandEvent &evt) {
    layout_box clipboard;
    if (!GetClipboard(clipboard)) return;
    
    if (clipboard.page != selected_page) {
        clipboard.page = selected_page;
    }
    
    int selection = m_list_boxes->GetSelection();
    layout_box_list::iterator selected = layout.end();
    if (selection >= 0) {
        selected = std::next(layout.begin(), selection);
    }
    auto &box = *layout.insert_after(selected, std::move(clipboard));
    updateLayout();
    selectBox(&box);
}

void frame_editor::OpenControlScript(wxCommandEvent &evt) {
    getControlScript(true);
}

void frame_editor::OnOpenLayoutOptions(wxCommandEvent &evt) {
    LayoutOptionsDialog(this, &layout).ShowModal();
}

void frame_editor::OnAutoLayout(wxCommandEvent &evt) {
    if (!m_doc.isopen()) {
        wxBell();
        return;
    }

    try {
        reader my_reader(m_doc);
        my_reader.add_layout(getControlScript().ToStdString());
        my_reader.add_flags(reader_flags::HALT_ON_SETLAYOUT);
        my_reader.start();

        auto &layouts = my_reader.get_layouts();
        if (layouts.size() <= 1) {
            wxMessageBox("Impossibile determinare il layout di questo file", "Errore", wxOK | wxICON_WARNING);
        } else if (saveIfModified()) {
            openFile(layouts.back().string());
        }
    } catch (const std::exception &error) {
        wxMessageBox(error.what(), "Errore", wxICON_ERROR);
    }
}

void frame_editor::OnRotate(wxCommandEvent &evt) {
    ++rotation %= 4;

    setSelectedPage(selected_page, true);
}

void frame_editor::OnLoadPdf(wxCommandEvent &evt) {
    wxString lastPdfDir = m_config->Read("LastPdfDir");
    wxFileDialog diag(this, "Apri PDF", lastPdfDir, wxEmptyString, "File PDF (*.pdf)|*.pdf|Tutti i file (*.*)|*.*");

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    m_config->Write("LastPdfDir", wxFileName(diag.GetPath()).GetPath());
    loadPdf(diag.GetPath().ToStdString());
    updateLayout(false);
}

void frame_editor::OnPageSelect(wxSpinEvent &evt) {
    OnPageEnter(evt);
}

void frame_editor::OnPageEnter(wxCommandEvent &evt) {
    if (!m_doc.isopen()) return;

    setSelectedPage(m_page->GetValue());
}

void frame_editor::OnChangeTool(wxCommandEvent &evt) {
    m_image->setSelectedTool(evt.GetId());
}

void frame_editor::OnSelectBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        auto it = std::next(layout.begin(), selection);
        selectBox(&*it);
    } else {
        selectBox(nullptr);
    }
}

void frame_editor::EditSelectedBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        auto it = std::next(layout.begin(), selection);
        box_dialog::openDialog(this, *it);
    }
}

void frame_editor::OnDelete(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        auto it = std::next(layout.begin(), selection);
        if (box_dialog::closeDialog(*it)) {
            layout.erase(it);
            updateLayout();
        }
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
        auto it = std::next(layout.begin(), selection);
        layout.splice(std::prev(it), layout, it);
        updateLayout();
        selectBox(&*it);
    }
}

void frame_editor::OnMoveDown(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int)layout.size() - 1) {
        auto it = std::next(layout.begin(), selection);
        layout.splice(it, layout, std::next(it));
        updateLayout();
        selectBox(&*it);
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