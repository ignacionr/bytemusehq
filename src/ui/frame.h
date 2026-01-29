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
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#endif

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

// SSH configuration for remote folder browsing
struct FrameSshConfig {
    bool enabled = false;
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        std::string cmd = "ssh";
        if (!extraOptions.empty()) cmd += " " + extraOptions;
        if (!identityFile.empty()) cmd += " -i \"" + identityFile + "\"";
        if (port != 22) cmd += " -p " + std::to_string(port);
        cmd += " -o ConnectTimeout=" + std::to_string(connectionTimeout);
        cmd += " -o BatchMode=yes";
        if (!user.empty()) cmd += " " + user + "@" + host;
        else cmd += " " + host;
        return cmd;
    }
    
    bool isValid() const { return enabled && !host.empty(); }
    
    static FrameSshConfig LoadFromConfig() {
        auto& config = Config::Instance();
        FrameSshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        return ssh;
    }
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
    wxString m_currentCategory;        // Currently selected category ID
    int m_themeListenerId;
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
    void PopulateTree(const wxString& path, wxTreeItemId parentItem);
    void PopulateTreeRemote(const wxString& path, wxTreeItemId parentItem);
    void UpdateTitle();
    
    FrameSshConfig m_sshConfig;  // SSH configuration for remote browsing
    bool m_isRemoteTree = false; // Whether current tree is remote
    
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
