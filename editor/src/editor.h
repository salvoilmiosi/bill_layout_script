#ifndef __EDITOR_H__
#define __EDITOR_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "layout.h"

#include <deque>
#include <memory>

enum {
    FIRST = 10000,
    
    MENU_NEW, MENU_OPEN, MENU_SAVE, MENU_SAVEAS, MENU_CLOSE,
    MENU_UNDO, MENU_REDO, MENU_CUT, MENU_COPY, MENU_PASTE,
    MENU_LOAD_PDF, MENU_EDITBOX, MENU_DELETE, MENU_READDATA,
    MENU_EDITCONTROL, MENU_LAYOUTPATH,

    MENU_OPEN_RECENT,
    MENU_OPEN_RECENT_END = MENU_OPEN_RECENT + 20,
    
    MENU_OPEN_PDF_RECENT,
    MENU_OPEN_PDF_RECENT_END = MENU_OPEN_PDF_RECENT + 20,

    CTL_LOAD_PDF, CTL_AUTO_LAYOUT, CTL_PAGE, CTL_SCALE,

    TOOL_SELECT, TOOL_NEWBOX, TOOL_DELETEBOX, TOOL_RESIZE, TOOL_MOVEUP, TOOL_MOVEDOWN,
    
    CTL_LIST_BOXES,
};

class frame_editor : public wxFrame {
public:
    frame_editor();

    int getSelectedPage() {
        return selected_page;
    }
    void setSelectedPage(int page, bool force = false);
    void selectBox(const box_ptr &box);

    void updateRecentFiles(bool save = false);
    
    void openFile(const wxString &filename);
    void loadPdf(const wxString &pdf_filename);
    void updateLayout(bool addToHistory = true);
    bool save(bool saveAs = false);
    bool saveIfModified();
    wxString getControlScript(bool open_dialog = false);
    wxString getLayoutPath(bool open_dialog = false);

    const pdf_document &getPdfDocument() {
        return m_doc;
    }

    bill_layout_script layout;

private:
    void OnNewFile      (wxCommandEvent &evt);
    void OnOpenFile     (wxCommandEvent &evt);
    void OnSaveFile     (wxCommandEvent &evt);
    void OnSaveFileAs   (wxCommandEvent &evt);
    void OnMenuClose    (wxCommandEvent &evt);
    void OnOpenRecent   (wxCommandEvent &evt);
    void OnOpenRecentPdf (wxCommandEvent &evt);
    void OnUndo         (wxCommandEvent &evt);
    void OnRedo         (wxCommandEvent &evt);
    void OnCut          (wxCommandEvent &evt);
    void OnCopy         (wxCommandEvent &evt);
    void OnPaste        (wxCommandEvent &evt);
    void OpenControlScript (wxCommandEvent &evt);
    void OpenLayoutPath (wxCommandEvent &evt);
    void OnAutoLayout   (wxCommandEvent &evt);
    void OnLoadPdf      (wxCommandEvent &evt);
    void OnPageSelect   (wxCommandEvent &evt);
    void OnScaleChange  (wxScrollEvent &evt);
    void OnScaleChangeFinal (wxScrollEvent &evt);
    void OnChangeTool   (wxCommandEvent &evt);
    void OnSelectBox    (wxCommandEvent &evt);
    void EditSelectedBox(wxCommandEvent &evt);
    void OnDelete       (wxCommandEvent &evt);
    void OnReadData     (wxCommandEvent &evt);
    void OnMoveUp       (wxCommandEvent &evt);
    void OnMoveDown     (wxCommandEvent &evt);
    void OnFrameClose   (wxCloseEvent &evt);

    DECLARE_EVENT_TABLE()

private:
    class box_editor_panel *m_image;

    wxMenu *m_recent;
    wxMenu *m_recent_pdfs;

    wxComboBox *m_page;
    wxSlider *m_scale;

    wxListBox *m_list_boxes;

    std::deque<bill_layout_script> history;
    std::deque<bill_layout_script>::iterator currentHistory;

    std::deque<wxString> recentFiles;
    std::deque<wxString> recentPdfs;

    bool modified = false;

private:
    pdf_document m_doc;
    wxString layout_filename;
    int selected_page = 0;
};

#endif