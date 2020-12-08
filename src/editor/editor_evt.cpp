#include "editor.h"

#include <json/json.h>

#include <wx/filename.h>
#include <wx/config.h>

#include "subprocess.h"
#include "utils.h"

#include "layout_ext.h"
#include "clipboard.h"
#include "compile_error_diag.h"
#include "box_editor_panel.h"
#include "box_dialog.h"
#include "output_dialog.h"

void frame_editor::OnNewFile(wxCommandEvent &evt) {
    if (saveIfModified()) {
        modified = false;
        layout_filename.clear();
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
    if (selection >= 0 && SetClipboard(*layout[selection])) {
        layout.erase(layout.begin() + selection);
        updateLayout();
    }
}

void frame_editor::OnCopy(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0) {
        SetClipboard(*layout[selection]);
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
    getControlScript();
}

void frame_editor::OpenLayoutPath(wxCommandEvent &evt) {
    getLayoutPath();
}

void frame_editor::OnCompile(wxCommandEvent &evt) {
    wxString filename_out;
    if (!layout_filename.empty()) filename_out = wxFileName::StripExtension(layout_filename) + ".out";
    wxFileDialog diag(this, "Compila Layout", wxEmptyString, filename_out, "File output (*.out)|*.out|Tutti i file (*.*)|*.*", wxFD_SAVE);

    if (diag.ShowModal() == wxID_CANCEL)
        return;

    wxString output_file = diag.GetPath().ToStdString();
    try {
        wxString cmd_str = get_app_path() + "layout_compiler";
        const char *args[] = {
            cmd_str.c_str(),
            "-o",
            output_file.c_str(),
            "-",
            nullptr
        };
        auto process = open_process(args);

        process->m_stdin << layout;
        process->close_stdin();
        
        std::string compile_output = read_all(process->m_stdout);
        if (!compile_output.empty()) {
            CompileErrorDialog(this, compile_output).ShowModal();
        }
    } catch (const process_error &error) {
        wxMessageBox(error.message, "Errore", wxICON_ERROR);
    }
}

void frame_editor::OnAutoLayout(wxCommandEvent &evt) {
    if (!m_doc.isopen()) {
        wxBell();
        return;
    }

    wxString control_script_filename = wxConfig::Get()->Read("ControlScriptFilename");
    if (control_script_filename.empty()) {
        control_script_filename = getControlScript();
        if (control_script_filename.empty()) return;
    }
    
    wxString cmd_str = get_app_path() + "layout_reader";
    wxString layout_path = wxConfig::Get()->Read("LayoutPath");
    if (layout_path.empty()) {
        layout_path = getLayoutPath();
        if (layout_path.empty()) return;
    }

    const char *args[] = {
        cmd_str.c_str(),
        "-p", m_doc.filename().c_str(),
        control_script_filename.c_str(),
        nullptr
    };

    auto process = open_process(args);

    Json::Value json_output;
    process->m_stdout >> json_output;

    if (json_output["error"].asBool()) {
        wxMessageBox("Impossibile leggere l'output: " + json_output["message"].asString(), "Errore", wxOK | wxICON_ERROR);
    } else {
        wxString output_layout = json_output["globals"]["layout"].asString();
        if (output_layout.empty()) {
            wxMessageBox("Impossibile determinare il layout di questo file", "Errore", wxOK | wxICON_WARNING);
        } else if (saveIfModified()) {
            openFile(layout_path + wxFileName::GetPathSeparator() + output_layout + ".bls");
        }
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
        if (selected_page > m_doc.num_pages() || selected_page <= 0) {
            wxBell();
        } else {
            setSelectedPage(selected_page);
        }
    } else {
        wxBell();
    }
}

void frame_editor::OnChangeTool(wxCommandEvent &evt) {
    m_image->setSelectedTool(evt.GetId());
}

void frame_editor::OnSelectBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        selectBox(layout[selection]);
    } else {
        selectBox(nullptr);
    }
}

void frame_editor::EditSelectedBox(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        new box_dialog(this, *layout[selection]);
    }
}

void frame_editor::OnDelete(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int) layout.size()) {
        layout.erase(layout.begin() + selection);
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
        std::swap(layout[selection], layout[selection-1]);
        updateLayout();
    }
}

void frame_editor::OnMoveDown(wxCommandEvent &evt) {
    int selection = m_list_boxes->GetSelection();
    if (selection >= 0 && selection < (int)layout.size() - 1) {
        std::swap(layout[selection], layout[selection+1]);
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