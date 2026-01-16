#ifndef FRAME_H
#define FRAME_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include "editor.h"
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
    Editor* GetEditor() { return m_editor; }
    wxStyledTextCtrl* GetTextCtrl() { return m_editor ? m_editor->GetTextCtrl() : nullptr; }

private:
    wxTreeCtrl* m_treeCtrl;
    Editor* m_editor;
    
    void SetupUI();
    void SetupMenuBar();
    void SetupAccelerators();
    void RegisterCommands();
    void PopulateTree(const wxString& path, wxTreeItemId parentItem);
    void UpdateTitle();
    
    // Command palette
    void ShowCommandPalette();
    CommandContext CreateCommandContext();
    
    // Event handlers
    void OnTreeItemActivated(wxTreeEvent& event);
    void OnTreeItemCollapsing(wxTreeEvent& event);
    void OnTreeItemExpanding(wxTreeEvent& event);
    void OnCommandPalette(wxCommandEvent& event);
    void OnNewFile(wxCommandEvent& event);
    void OnOpenFile(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnSaveAs(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    wxDECLARE_EVENT_TABLE();
};

#endif // FRAME_H
