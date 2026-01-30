#ifndef REMOTE_FOLDER_DIALOG_H
#define REMOTE_FOLDER_DIALOG_H

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <sstream>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#endif

namespace UI {

/**
 * SSH configuration for the remote folder dialog.
 */
struct RemoteFolderSshConfig {
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    
    std::string buildSshPrefix() const {
        if (host.empty()) return "";
        
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
        return !host.empty();
    }
};

/**
 * Remote folder browser dialog.
 * Allows browsing directories on a remote machine via SSH.
 */
class RemoteFolderDialog : public wxDialog {
public:
    RemoteFolderDialog(wxWindow* parent, const RemoteFolderSshConfig& sshConfig,
                       const wxString& initialPath = "~")
        : wxDialog(parent, wxID_ANY, "Open Remote Folder", 
                   wxDefaultPosition, wxSize(500, 450),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , m_sshConfig(sshConfig)
        , m_currentPath(initialPath)
    {
        CreateControls();
        NavigateTo(m_currentPath);
    }
    
    /**
     * Get the selected folder path.
     */
    wxString GetPath() const {
        return m_currentPath;
    }
    
private:
    RemoteFolderSshConfig m_sshConfig;
    wxString m_currentPath;
    
    wxTextCtrl* m_pathCtrl = nullptr;
    wxListCtrl* m_listCtrl = nullptr;
    wxButton* m_upBtn = nullptr;
    wxButton* m_homeBtn = nullptr;
    wxButton* m_refreshBtn = nullptr;
    wxStaticText* m_statusText = nullptr;
    
    struct DirEntry {
        wxString name;
        bool isDirectory;
    };
    std::vector<DirEntry> m_entries;
    
    void CreateControls() {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // Connection info
        wxString connInfo = wxString::Format("Remote: %s@%s", 
            m_sshConfig.user.empty() ? "user" : m_sshConfig.user,
            m_sshConfig.host);
        wxStaticText* connLabel = new wxStaticText(this, wxID_ANY, connInfo);
        wxFont boldFont = connLabel->GetFont();
        boldFont.SetWeight(wxFONTWEIGHT_BOLD);
        connLabel->SetFont(boldFont);
        mainSizer->Add(connLabel, 0, wxALL | wxEXPAND, 10);
        
        // Path bar with navigation buttons
        wxBoxSizer* pathSizer = new wxBoxSizer(wxHORIZONTAL);
        
        m_upBtn = new wxButton(this, wxID_ANY, wxT("\u2191"), wxDefaultPosition, wxSize(30, -1));  // ↑
        m_upBtn->SetToolTip("Go to parent folder");
        pathSizer->Add(m_upBtn, 0, wxRIGHT, 2);
        
        m_homeBtn = new wxButton(this, wxID_ANY, wxT("\u2302"), wxDefaultPosition, wxSize(30, -1));  // ⌂
        m_homeBtn->SetToolTip("Go to home folder");
        pathSizer->Add(m_homeBtn, 0, wxRIGHT, 5);
        
        m_pathCtrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                    wxTE_PROCESS_ENTER);
        m_pathCtrl->SetToolTip("Current path - edit and press Enter to navigate");
        pathSizer->Add(m_pathCtrl, 1, wxRIGHT, 5);
        
        m_refreshBtn = new wxButton(this, wxID_ANY, wxT("\u21BB"), wxDefaultPosition, wxSize(30, -1));  // ↻
        m_refreshBtn->SetToolTip("Refresh");
        pathSizer->Add(m_refreshBtn, 0);
        
        mainSizer->Add(pathSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
        
        // Directory listing
        m_listCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxLC_REPORT | wxLC_SINGLE_SEL);
        m_listCtrl->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 300);
        m_listCtrl->InsertColumn(1, "Type", wxLIST_FORMAT_LEFT, 100);
        mainSizer->Add(m_listCtrl, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);
        
        // Status bar
        m_statusText = new wxStaticText(this, wxID_ANY, "");
        mainSizer->Add(m_statusText, 0, wxALL | wxEXPAND, 10);
        
        // Buttons
        wxStdDialogButtonSizer* btnSizer = new wxStdDialogButtonSizer();
        wxButton* okBtn = new wxButton(this, wxID_OK, "Select Folder");
        wxButton* cancelBtn = new wxButton(this, wxID_CANCEL, "Cancel");
        btnSizer->AddButton(okBtn);
        btnSizer->AddButton(cancelBtn);
        btnSizer->Realize();
        mainSizer->Add(btnSizer, 0, wxALL | wxALIGN_RIGHT, 10);
        
        SetSizer(mainSizer);
        
        // Bind events
        m_upBtn->Bind(wxEVT_BUTTON, &RemoteFolderDialog::OnUpClicked, this);
        m_homeBtn->Bind(wxEVT_BUTTON, &RemoteFolderDialog::OnHomeClicked, this);
        m_refreshBtn->Bind(wxEVT_BUTTON, &RemoteFolderDialog::OnRefreshClicked, this);
        m_pathCtrl->Bind(wxEVT_TEXT_ENTER, &RemoteFolderDialog::OnPathEnter, this);
        m_listCtrl->Bind(wxEVT_LIST_ITEM_ACTIVATED, &RemoteFolderDialog::OnItemActivated, this);
        m_listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &RemoteFolderDialog::OnItemSelected, this);
    }
    
    /**
     * Navigate to a remote directory and populate the list.
     */
    void NavigateTo(const wxString& path) {
        m_statusText->SetLabel("Loading...");
        m_listCtrl->DeleteAllItems();
        m_entries.clear();
        Update();
        
        // Resolve path (handle ~ and relative paths)
        wxString resolvedPath = ResolvePath(path);
        if (resolvedPath.IsEmpty()) {
            m_statusText->SetLabel("Failed to resolve path");
            return;
        }
        
        // Get directory listing via SSH
        std::string sshPrefix = m_sshConfig.buildSshPrefix();
        std::string cmd = sshPrefix + " \"ls -la '" + resolvedPath.ToStdString() + "' 2>&1\" 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            m_statusText->SetLabel("Failed to connect via SSH");
            return;
        }
        
        std::string output;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        int status = pclose(pipe);
        
        // Check for errors
        if (output.find("No such file or directory") != std::string::npos ||
            output.find("Permission denied") != std::string::npos ||
            output.find("Not a directory") != std::string::npos) {
            m_statusText->SetLabel(wxString(output.substr(0, 100)));
            return;
        }
        
        // Parse ls -la output
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Skip total line and empty lines
            if (line.empty() || line.find("total") == 0) continue;
            
            // Parse ls -la output
            std::istringstream lineStream(line);
            std::string permissions, links, owner, group, size, month, day, timeStr;
            lineStream >> permissions >> links >> owner >> group >> size >> month >> day >> timeStr;
            
            std::string name;
            std::getline(lineStream, name);
            
            // Trim leading whitespace from name
            size_t start = name.find_first_not_of(" \t");
            if (start != std::string::npos) {
                name = name.substr(start);
            }
            
            // Skip . and hidden files (but keep ..)
            if (name.empty() || name == "." || (name[0] == '.' && name != "..")) continue;
            
            bool isDirectory = !permissions.empty() && permissions[0] == 'd';
            
            // Only show directories (this is a folder browser)
            if (isDirectory) {
                DirEntry entry;
                entry.name = wxString(name);
                entry.isDirectory = true;
                m_entries.push_back(entry);
            }
        }
        
        // Sort entries (.. first, then alphabetically)
        std::sort(m_entries.begin(), m_entries.end(), [](const DirEntry& a, const DirEntry& b) {
            if (a.name == "..") return true;
            if (b.name == "..") return false;
            return a.name.CmpNoCase(b.name) < 0;
        });
        
        // Populate list
        for (size_t i = 0; i < m_entries.size(); ++i) {
            long idx = m_listCtrl->InsertItem(i, m_entries[i].name);
            m_listCtrl->SetItem(idx, 1, "Folder");
        }
        
        m_currentPath = resolvedPath;
        m_pathCtrl->SetValue(m_currentPath);
        m_statusText->SetLabel(wxString::Format("%zu folder(s)", m_entries.size() - (m_entries.empty() ? 0 : (m_entries[0].name == ".." ? 1 : 0))));
    }
    
    /**
     * Resolve a path on the remote machine (handles ~ expansion).
     */
    wxString ResolvePath(const wxString& path) {
        std::string sshPrefix = m_sshConfig.buildSshPrefix();
        
        // If the path starts with '~', use remote expansion (tilde isn't expanded
        // when placed inside single-quotes). Otherwise use cd to resolve the
        // absolute path. This avoids treating '~' as a literal directory name.
        std::string cmd;
        if (!path.IsEmpty() && path[0] == '~') {
            // Use eval to expand the tilde on the remote side
            cmd = sshPrefix + " \"eval echo " + path.ToStdString() + "\" 2>/dev/null";
        } else {
            // Quote the path for safety when it doesn't need tilde expansion
            cmd = sshPrefix + " \"cd '" + path.ToStdString() + "' 2>/dev/null && pwd\" 2>&1";
        }

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";

        std::string output;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        pclose(pipe);
        
        // Trim whitespace
        size_t start = output.find_first_not_of(" \t\n\r");
        size_t end = output.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            return wxString(output.substr(start, end - start + 1));
        }
        
        return "";
    }
    
    /**
     * Get parent directory path.
     */
    wxString GetParentPath(const wxString& path) {
        if (path == "/" || path.IsEmpty()) return "/";
        
        wxString p = path;
        if (p.EndsWith("/")) {
            p = p.RemoveLast();
        }
        
        size_t lastSlash = p.rfind('/');
        if (lastSlash == 0) return "/";
        if (lastSlash == wxString::npos) return "/";
        
        return p.substr(0, lastSlash);
    }
    
    void OnUpClicked(wxCommandEvent&) {
        NavigateTo(GetParentPath(m_currentPath));
    }
    
    void OnHomeClicked(wxCommandEvent&) {
        NavigateTo("~");
    }
    
    void OnRefreshClicked(wxCommandEvent&) {
        NavigateTo(m_currentPath);
    }
    
    void OnPathEnter(wxCommandEvent&) {
        NavigateTo(m_pathCtrl->GetValue());
    }
    
    void OnItemActivated(wxListEvent& event) {
        long idx = event.GetIndex();
        if (idx < 0 || idx >= (long)m_entries.size()) return;
        
        const DirEntry& entry = m_entries[idx];
        
        if (entry.name == "..") {
            NavigateTo(GetParentPath(m_currentPath));
        } else {
            wxString newPath = m_currentPath;
            if (!newPath.EndsWith("/")) newPath += "/";
            newPath += entry.name;
            NavigateTo(newPath);
        }
    }
    
    void OnItemSelected(wxListEvent& event) {
        // Could update status or preview here
        (void)event;
    }
};

} // namespace UI

#endif // REMOTE_FOLDER_DIALOG_H
