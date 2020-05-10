#ifndef __EDITOR_H__
#define __EDITOR_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "../shared/layout.h"

#include <deque>
#include <memory>

enum {
    FIRST = 10000,
    
    MENU_NEW, MENU_OPEN, MENU_SAVE, MENU_SAVEAS, MENU_CLOSE,
    MENU_UNDO, MENU_REDO, MENU_CUT, MENU_COPY, MENU_PASTE,
    MENU_LOAD_PDF, MENU_EDITBOX, MENU_DELETE, MENU_READDATA,

    CTL_LOAD_PDF, CTL_PAGE, CTL_SCALE,

    TOOL_SELECT, TOOL_NEWBOX, TOOL_DELETEBOX, TOOL_MOVEUP, TOOL_MOVEDOWN,
    
    CTL_LIST_BOXES,
};

class frame_editor : public wxFrame {
public:
    frame_editor();

    int getSelectedPage() {
        return selected_page;
    }
    void setSelectedPage(int page, bool force = false);
    void selectBox(int id);

    void openFile(const std::string &filename);
    void loadPdf(const std::string &pdf_filename);
    void updateLayout(bool addToHistory = true);
    bool save(bool saveAs = false);
    bool saveIfModified();

    layout_bolletta layout;

private:
    void OnNewFile      (wxCommandEvent &evt);
    void OnOpenFile     (wxCommandEvent &evt);
    void OnSaveFile     (wxCommandEvent &evt);
    void OnSaveFileAs   (wxCommandEvent &evt);
    void OnMenuClose    (wxCommandEvent &evt);
    void OnUndo         (wxCommandEvent &evt);
    void OnRedo         (wxCommandEvent &evt);
    void OnCut          (wxCommandEvent &evt);
    void OnCopy         (wxCommandEvent &evt);
    void OnPaste        (wxCommandEvent &evt);
    void OnLoadPdf      (wxCommandEvent &evt);
    void OnPageSelect   (wxCommandEvent &evt);
    void OnScaleChange  (wxCommandEvent &evt);
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

    wxComboBox *m_page;
    wxSlider *m_scale;

    wxListBox *m_list_boxes;

    std::deque<layout_bolletta> history;
    std::deque<layout_bolletta>::iterator currentHistory;
    bool modified = false;

    std::unique_ptr<layout_box> clipboard;

private:
    xpdf::pdf_info info;
    std::string layout_filename{};
    int selected_page = 0;
};

const std::string &get_app_path();

#endif