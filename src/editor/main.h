#ifndef __MAIN_H__
#define __MAIN_H__

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "box_editor_panel.h"

#include "../shared/layout.h"

#include <vector>

enum {
    FIRST = 10000,
    
    MENU_NEW, MENU_OPEN, MENU_SAVE, MENU_SAVEAS, MENU_CLOSE,
    MENU_UNDO, MENU_REDO, MENU_CUT, MENU_COPY, MENU_PASTE,

    TOOL_NEW, TOOL_OPEN, TOOL_SAVE,
    TOOL_UNDO, TOOL_REDO, TOOL_CUT, TOOL_COPY, TOOL_PASTE,

    CTL_LOAD_PDF, CTL_PAGE, CTL_SCALE,

    TOOL_SELECT, TOOL_NEWBOX, TOOL_DELETEBOX,
    
    CTL_LIST_BOXES,
};

class MainApp : public wxApp {
public:
    virtual bool OnInit() override;

public:
    void setSelectedPage(int page);
    void updateBoxList();
    void selectBox(int id);

private:
    void OnNewFile      (wxCommandEvent &evt);
    void OnOpenFile     (wxCommandEvent &evt);
    void OnSaveFile     (wxCommandEvent &evt);
    void OnSaveFileAs   (wxCommandEvent &evt);
    void OnClose        (wxCommandEvent &evt);
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

    DECLARE_EVENT_TABLE()

private:
    wxFrame *m_frame;
    box_editor_panel *m_image;

    wxComboBox *m_page;
    wxSlider *m_scale;

    wxListBox *m_list_boxes;

    layout_bolletta layout;

private:
    std::string app_path;
    xpdf::pdf_info info;
    std::string pdf_filename{};
    int selected_tool = TOOL_SELECT;
    int selected_page = 0;

    friend class box_editor_panel;
};

#endif