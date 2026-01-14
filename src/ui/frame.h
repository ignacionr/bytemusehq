#ifndef FRAME_H
#define FRAME_H

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>

// Custom tree item data to store file paths
class PathData : public wxTreeItemData {
public:
    PathData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }

private:
    wxString m_path;
};

class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    wxTreeCtrl* m_treeCtrl;
    wxStyledTextCtrl* m_editor;
    
    void SetupUI();
    void PopulateTree(const wxString& path, wxTreeItemId parentItem);
    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeItemCollapsing(wxTreeEvent& event);
    void OnTreeItemExpanding(wxTreeEvent& event);
    
    wxDECLARE_EVENT_TABLE();
};

#endif // FRAME_H
