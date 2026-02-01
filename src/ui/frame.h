#ifndef FRAME_H
#define FRAME_H

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/splitter.h>
#include "editor.h"
#include "terminal.h"
#include "widget.h"
#include "widget_bar.h"
#include "widget_activity_bar.h"
#include "../commands/command.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../fs/fs.h"
#include <sstream>

// Forward declarations
class WidgetActivityBar;

namespace BuiltinWidgets {
    class SymbolsWidget;
}

// Custom tree item data to store file paths
class PathData : public wxTreeItemData {
public:
    PathData(const wxString& path, bool isRemote = false) 
        : m_path(path), m_isRemote(isRemote) {}
    const wxString& GetPath() const { return m_path; }
    bool IsRemote() const { return m_isRemote; }

private:
    wxString m_path;
    bool m_isRemote;
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
    
    // Open a folder in the tree and change working directory
    void OpenFolder(const wxString& path, bool isRemote = false);
    
    // Terminal visibility
    void ShowTerminal(bool show = true);
    void ToggleTerminal();
    bool IsTerminalVisible() const;
    
    // Sidebar widget management (generic)
    void ShowSidebarWidget(const wxString& widgetId, bool show = true);
    void ToggleSidebarWidget(const wxString& widgetId);
    bool IsSidebarWidgetVisible(const wxString& widgetId) const;
    
    // Widget system access
    WidgetContext& GetWidgetContext() { return m_widgetContext; }
    void NotifyThemeChanged();
    
    // Remote connection status
    bool IsConnectedToRemote() const;
    wxString GetRemoteHostInfo() const;
    
    // MCP provider management
    void ReinitializeMCPProviders();

private:
    wxTreeCtrl* m_treeCtrl;
    Editor* m_editor;
    Terminal* m_terminal;
    wxSplitterWindow* m_rightSplitter; // Vertical splitter for editor/terminal
    wxSplitterWindow* m_hSplitter;     // Horizontal splitter (left area | right)
    wxSplitterWindow* m_leftSplitter;  // Vertical splitter for tree/widget bar
    wxPanel* m_mainPanel;
    wxPanel* m_leftPanel;              // Contains activity bar + tree
    wxPanel* m_leftContentPanel;       // Tree control area
    WidgetActivityBar* m_activityBar;  // Activity bar with category buttons
    WidgetBar* m_widgetBar;            // Widget bar for sidebar widgets
    wxStatusBar* m_statusBar;          // Status bar for remote connection status
    wxString m_currentCategory;        // Currently selected category ID
    int m_themeListenerId;
    int m_configListenerId;            // Config change listener for SSH settings
    WidgetContext m_widgetContext;
    
    void SetupUI();
    void SetupActivityBar();           // Initialize the activity bar with categories
    void SetupSidebarWidgets();
    void ConnectCodeIndexToMCP(BuiltinWidgets::SymbolsWidget* symbolsWidget);  // Connect to MCP
    void UpdateWidgetBarVisibility();  // Show/hide widget bar based on visible widgets
    void OnCategorySelected(const wxString& categoryId);  // Handle category selection
    void SetupMenuBar();
    void SetupAccelerators();
    void RegisterCommands();
    void RegisterWidgets();
    void PopulateTree(const wxTreeItemId& parentItem);
    void UpdateTitle();
    void UpdateStatusBar();            // Update status bar with connection info
    
    FS::Filesystem m_filesystem;       // Unified filesystem access (local or remote)
    
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
