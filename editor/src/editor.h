#ifndef __EDITOR_H__
#define __EDITOR_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/spinctrl.h>
#include <wx/config.h>
#include <wx/filehistory.h>

#include "layout.h"

#include <deque>

constexpr size_t MAX_RECENT_FILES_HISTORY = 10;
constexpr size_t MAX_RECENT_PDFS_HISTORY = 10;

enum {
    FIRST = 10000,
    
    MENU_NEW, MENU_OPEN, MENU_SAVE, MENU_SAVEAS, MENU_CLOSE,
    MENU_UNDO, MENU_REDO, MENU_CUT, MENU_COPY, MENU_PASTE,
    MENU_LOAD_PDF, MENU_EDITBOX, MENU_DELETE, MENU_READDATA,
    MENU_EDITCONTROL, MENU_SETLANGUAGE,

    MENU_OPEN_RECENT,
    MENU_OPEN_RECENT_END = MENU_OPEN_RECENT + MAX_RECENT_FILES_HISTORY,
    
    MENU_OPEN_PDF_RECENT,
    MENU_OPEN_PDF_RECENT_END = MENU_OPEN_PDF_RECENT + MAX_RECENT_PDFS_HISTORY,

    CTL_ROTATE, CTL_LOAD_PDF, CTL_AUTO_LAYOUT, CTL_PAGE, CTL_SCALE,

    TOOL_SELECT, TOOL_NEWBOX, TOOL_DELETEBOX, TOOL_RESIZE, TOOL_TEST, TOOL_MOVEUP, TOOL_MOVEDOWN,
    
    CTL_LIST_BOXES,
};

class frame_editor : public wxFrame {
public:
    frame_editor();

    int getSelectedPage() {
        return selected_page;
    }
    void setSelectedPage(int page, bool force = false);
    void selectBox(layout_box *box);
    
    void openFile(const wxString &filename);
    void loadPdf(const wxString &pdf_filename);
    void updateLayout(bool addToHistory = true);
    bool save(bool saveAs = false);
    bool saveIfModified();
    wxString getControlScript(bool open_dialog = false);

    const pdf_document &getPdfDocument() {
        return m_doc;
    }

    int getBoxRotation() {
        return (4 - rotation) % 4;
    }

    layout_box_list layout;
    std::filesystem::path m_filename;

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
    void OnSetLanguage  (wxCommandEvent &evt);
    void OnAutoLayout   (wxCommandEvent &evt);
    void OnRotate       (wxCommandEvent &evt);
    void OnLoadPdf      (wxCommandEvent &evt);
    void OnPageSelect   (wxSpinEvent &evt);
    void OnPageEnter    (wxCommandEvent &evt);
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

    wxConfig *m_config;

    wxFileHistory *m_bls_history;
    wxMenu *m_bls_history_menu;
    
    wxFileHistory *m_pdf_history;
    wxMenu *m_pdf_history_menu;

    wxSpinCtrl *m_page;
    wxSlider *m_scale;

    wxListBox *m_list_boxes;

    std::deque<layout_box_list> history;
    std::deque<layout_box_list>::iterator currentHistory;

    bool modified = false;
    int rotation = 0;

private:
    pdf_document m_doc;
    int selected_page = 0;
};

#endif