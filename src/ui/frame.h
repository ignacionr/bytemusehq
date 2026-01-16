#ifndef FRAME_H
#define FRAME_H

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <wx/treectrl.h>
#include "../commands/command.h"

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

    // Get the editor component (for commands)
    wxStyledTextCtrl* GetEditor() { return m_editor; }

private:
    wxTreeCtrl* m_treeCtrl;
    wxStyledTextCtrl* m_editor;
    wxString m_currentFilePath;
    
    void SetupUI();
    void SetupAccelerators();
    void RegisterCommands();
    void PopulateTree(const wxString& path, wxTreeItemId parentItem);
    
    // Command palette
    void ShowCommandPalette();
    CommandContext CreateCommandContext();
    
    // Event handlers
    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeItemCollapsing(wxTreeEvent& event);
    void OnTreeItemExpanding(wxTreeEvent& event);
    void OnCommandPalette(wxCommandEvent& event);
    
    wxDECLARE_EVENT_TABLE();
};

#endif // FRAME_H
