#ifndef FILE_TREE_WIDGET_H
#define FILE_TREE_WIDGET_H

#include "widget.h"
#include "editor.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

namespace BuiltinWidgets {

/**
 * SSH configuration for remote file tree browsing.
 */
struct FileTreeSshConfig {
    bool enabled = false;
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    std::string remotePath;
    
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        
        std::string cmd = "ssh";
        
        if (!extraOptions.empty()) {
            cmd += " " + extraOptions;
        }
        
        if (!identityFile.empty()) {
            cmd += " -i \"" + identityFile + "\"";
        }
        
        if (port != 22) {
            cmd += " -p " + std::to_string(port);
        }
        
        cmd += " -o ConnectTimeout=" + std::to_string(connectionTimeout);
        cmd += " -o BatchMode=yes";
        
        if (!user.empty()) {
            cmd += " " + user + "@" + host;
        } else {
            cmd += " " + host;
        }
        
        return cmd;
    }
    
    bool isValid() const {
        return enabled && !host.empty();
    }
    
    static FileTreeSshConfig LoadFromConfig() {
        auto& config = Config::Instance();
        FileTreeSshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        ssh.remotePath = config.GetString("ssh.remotePath", "~").ToStdString();
        return ssh;
    }
};

/**
 * Tree item data to store file paths.
 */
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

/**
 * File tree sidebar widget.
 * Displays the workspace directory structure for file navigation.
 * Supports both local and remote (SSH) file browsing.
 */
class FileTreeWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.fileTree";
        info.name = "File Explorer";
        info.description = "Browse and open files in the workspace";
        info.location = WidgetLocation::Sidebar;
        info.category = WidgetCategories::Explorer();
        info.priority = 100;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        m_treeCtrl = new wxTreeCtrl(m_panel, wxID_ANY);
        sizer->Add(m_treeCtrl, 1, wxEXPAND);
        m_panel->SetSizer(sizer);
        
        // Store context for callbacks
        m_context = &context;
        
        // Load SSH configuration
        m_sshConfig = FileTreeSshConfig::LoadFromConfig();
        
        // Determine root directory
        wxString rootDir;
        wxString displayName;
        
        if (m_sshConfig.isValid()) {
            rootDir = wxString(m_sshConfig.remotePath);
            displayName = wxString::Format("[SSH] %s:%s", 
                wxString(m_sshConfig.host), rootDir);
        } else {
            rootDir = wxGetCwd();
            displayName = rootDir;
        }
        
        wxTreeItemId rootId = m_treeCtrl->AddRoot(displayName);
        m_treeCtrl->SetItemData(rootId, new PathData(rootDir, m_sshConfig.isValid()));
        
        if (m_sshConfig.isValid()) {
            PopulateTreeRemote(rootDir, rootId);
        } else {
            PopulateTree(rootDir, rootId);
        }
        m_treeCtrl->Expand(rootId);
        
        // Bind events
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &FileTreeWidget::OnItemActivated, this);
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_EXPANDING, &FileTreeWidget::OnItemExpanding, this);
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme || !m_panel || !m_treeCtrl) return;
        
        m_panel->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetForegroundColour(theme->ui.sidebarForeground);
        m_panel->Refresh();
    }

    wxTreeCtrl* GetTreeCtrl() { return m_treeCtrl; }

    std::vector<wxString> GetCommands() const override {
        return {
            "fileTree.refresh",
            "fileTree.collapseAll"
        };
    }

private:
    wxPanel* m_panel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;
    WidgetContext* m_context = nullptr;
    FileTreeSshConfig m_sshConfig;

    void PopulateTree(const wxString& path, wxTreeItemId parentItem) {
        wxDir dir(path);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        bool cont = dir.GetFirst(&filename);
        
        while (cont) {
            if (!filename.StartsWith(".")) {
                wxString fullPath = wxFileName(path, filename).GetFullPath();
                
                if (wxDir::Exists(fullPath)) {
                    wxTreeItemId itemId = m_treeCtrl->AppendItem(
                        parentItem, filename, -1, -1, new PathData(fullPath, false));
                    m_treeCtrl->AppendItem(itemId, ""); // Dummy for expand arrow
                } else {
                    m_treeCtrl->AppendItem(
                        parentItem, filename, -1, -1, new PathData(fullPath, false));
                }
            }
            cont = dir.GetNext(&filename);
        }
        m_treeCtrl->SortChildren(parentItem);
    }
    
    /**
     * Populate tree with remote directory contents via SSH.
     */
    void PopulateTreeRemote(const wxString& path, wxTreeItemId parentItem) {
        std::string sshPrefix = m_sshConfig.buildSshPrefix();
        std::string cmd = sshPrefix + " \"ls -la '" + path.ToStdString() + "' 2>/dev/null\" 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return;
        
        std::string output;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        pclose(pipe);
        
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Skip total line and empty lines
            if (line.empty() || line.find("total") == 0) continue;
            
            // Parse ls -la output
            std::istringstream lineStream(line);
            std::string permissions, links, owner, group, size, month, day, timeStr, name;
            lineStream >> permissions >> links >> owner >> group >> size >> month >> day >> timeStr;
            std::getline(lineStream, name);
            
            // Trim leading whitespace from name
            size_t start = name.find_first_not_of(" \t");
            if (start != std::string::npos) {
                name = name.substr(start);
            }
            
            // Skip . and .. and hidden files
            if (name.empty() || name == "." || name == ".." || name[0] == '.') continue;
            
            wxString fullPath = path;
            if (!fullPath.EndsWith("/")) fullPath += "/";
            fullPath += wxString(name);
            
            bool isDirectory = !permissions.empty() && permissions[0] == 'd';
            
            if (isDirectory) {
                wxTreeItemId itemId = m_treeCtrl->AppendItem(
                    parentItem, wxString(name), -1, -1, new PathData(fullPath, true));
                m_treeCtrl->AppendItem(itemId, ""); // Dummy for expand arrow
            } else {
                m_treeCtrl->AppendItem(
                    parentItem, wxString(name), -1, -1, new PathData(fullPath, true));
            }
        }
        m_treeCtrl->SortChildren(parentItem);
    }

    void OnItemActivated(wxTreeEvent& event) {
        wxTreeItemId itemId = event.GetItem();
        PathData* data = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
        if (!data) return;
        
        wxString path = data->GetPath();
        
        if (data->IsRemote()) {
            // For remote files, check if it's a directory via SSH
            std::string sshPrefix = m_sshConfig.buildSshPrefix();
            std::string testCmd = sshPrefix + " \"test -d '" + path.ToStdString() + "'\" 2>&1";
            int result = system(testCmd.c_str());
            
            if (result != 0) {
                // It's a file - open it in editor (will need to fetch content)
                if (m_context) {
                    auto* editor = m_context->Get<Editor>("editorComponent");
                    if (editor) {
                        // For remote files, we need to fetch content via SSH
                        // The editor should handle this - for now mark it as remote
                        editor->OpenRemoteFile(path, m_sshConfig.buildSshPrefix());
                    }
                }
            }
        } else {
            // Local file handling
            if (!wxDir::Exists(path) && wxFileExists(path)) {
                // Open file in editor
                if (m_context) {
                    auto* editor = m_context->Get<Editor>("editorComponent");
                    if (editor) {
                        editor->OpenFile(path);
                    }
                }
            }
        }
    }

    void OnItemExpanding(wxTreeEvent& event) {
        wxTreeItemId itemId = event.GetItem();
        wxTreeItemIdValue cookie;
        wxTreeItemId firstChild = m_treeCtrl->GetFirstChild(itemId, cookie);
        
        if (firstChild.IsOk()) {
            PathData* childData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(firstChild));
            if (!childData) {
                // Dummy child - delete and populate
                m_treeCtrl->Delete(firstChild);
                PathData* parentData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
                if (parentData) {
                    if (parentData->IsRemote()) {
                        PopulateTreeRemote(parentData->GetPath(), itemId);
                    } else {
                        PopulateTree(parentData->GetPath(), itemId);
                    }
                }
            }
        }
    }
};

} // namespace BuiltinWidgets

#endif // FILE_TREE_WIDGET_H
