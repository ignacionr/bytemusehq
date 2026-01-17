#ifndef FRAME_H
#define FRAME_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include "editor.h"
#include "terminal.h"
#include "../commands/command.h"
#include "../theme/theme.h"

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
    ~MainFrame();

    // Get the editor component (for commands)
    Editor* GetEditor() { return m_editor; }
    wxStyledTextCtrl* GetTextCtrl() { return m_editor ? m_editor->GetTextCtrl() : nullptr; }
    
    // Get the terminal component
    Terminal* GetTerminal() { return m_terminal; }
    
    // Terminal visibility
    void ShowTerminal(bool show = true);
    void ToggleTerminal();
    bool IsTerminalVisible() const;

private:
    wxTreeCtrl* m_treeCtrl;
    Editor* m_editor;
    Terminal* m_terminal;
    wxSplitterWindow* m_rightSplitter; // Vertical splitter for editor/terminal
    wxSplitterWindow* m_hSplitter;     // Horizontal splitter (tree | right)
    wxPanel* m_mainPanel;
    wxPanel* m_leftPanel;
    int m_themeListenerId;
    
    void SetupUI();
    void SetupMenuBar();
    void SetupAccelerators();
    void RegisterCommands();
    void PopulateTree(const wxString& path, wxTreeItemId parentItem);
    void UpdateTitle();
    
    // Theme support
    void ApplyTheme(const ThemePtr& theme);
    void ApplyCurrentTheme();
    
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
    void OnToggleTerminal(wxCommandEvent& event);
    
    wxDECLARE_EVENT_TABLE();
};

#endif // FRAME_H
