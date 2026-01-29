#ifndef TERMINAL_H
#define TERMINAL_H

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <memory>
#include "../theme/theme.h"

/**
 * SSH connection configuration.
 * When enabled, the terminal connects to a remote machine via SSH.
 */
struct SshConfig {
    bool enabled = false;
    wxString host;                          // Hostname or IP
    int port = 22;                          // SSH port
    wxString user;                          // SSH username
    wxString identityFile;                  // Path to private key (optional)
    wxString remotePath = "~";              // Remote working directory
    wxString extraOptions;                  // Additional SSH options
    bool forwardAgent = false;              // SSH agent forwarding
    int connectionTimeout = 30;             // Connection timeout in seconds
    
    // Build the SSH command to connect
    wxString BuildSshCommand() const {
        wxString cmd = "ssh";
        
        // Add options
        if (!extraOptions.IsEmpty()) {
            cmd += " " + extraOptions;
        }
        
        if (forwardAgent) {
            cmd += " -A";
        }
        
        if (!identityFile.IsEmpty()) {
            cmd += " -i \"" + identityFile + "\"";
        }
        
        if (port != 22) {
            cmd += wxString::Format(" -p %d", port);
        }
        
        cmd += wxString::Format(" -o ConnectTimeout=%d", connectionTimeout);
        
        // Add user@host
        if (!user.IsEmpty()) {
            cmd += " " + user + "@" + host;
        } else {
            cmd += " " + host;
        }
        
        return cmd;
    }
    
    // Check if configuration is valid for connection
    bool IsValid() const {
        return enabled && !host.IsEmpty();
    }
};

/**
 * Terminal component for ByteMuseHQ.
 * Provides a simple command-line interface with a persistent shell session.
 * Supports both local and remote (SSH) shell sessions.
 */
class Terminal : public wxPanel {
public:
    Terminal(wxWindow* parent, wxWindowID id = wxID_ANY);
    virtual ~Terminal();

    // Execute a command in the shell
    void ExecuteCommand(const wxString& command);
    
    // Clear the output
    void Clear();
    
    // Get the current working directory
    wxString GetWorkingDirectory() const { return m_workingDir; }
    
    // Set working directory
    void SetWorkingDirectory(const wxString& dir);
    
    // Theme support
    void ApplyTheme(const ThemePtr& theme);
    
    // SSH support
    void SetSshConfig(const SshConfig& config);
    SshConfig GetSshConfig() const { return m_sshConfig; }
    bool IsRemoteSession() const { return m_sshConfig.enabled && m_sshConfig.IsValid(); }
    
    // Reconnect (restart shell with current configuration)
    void Reconnect();

private:
    wxTextCtrl* m_output;       // Output display
    wxTextCtrl* m_input;        // Command input
    wxStaticText* m_label;      // Header label
    wxStaticText* m_prompt;     // Input prompt
    wxString m_workingDir;      // Current working directory (local or remote)
    int m_themeListenerId;
    
    // SSH configuration
    SshConfig m_sshConfig;
    
    // Shell process management
    wxProcess* m_process;
    long m_pid;
    wxOutputStream* m_processInput;
    wxInputStream* m_processOutput;
    wxInputStream* m_processError;
    
    // Command history
    wxArrayString m_history;
    int m_historyIndex;
    
    // Setup methods
    void SetupUI();
    void StartShell();
    void StopShell();
    void ApplyCurrentTheme();
    
    // Read output from shell
    void ReadProcessOutput();
    
    // Get the shell command for the current platform
    static wxString GetShellCommand();
    
    // Event handlers
    void OnInputEnter(wxCommandEvent& event);
    void OnInputKeyDown(wxKeyEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnProcessTerminated(wxProcessEvent& event);

    wxDECLARE_EVENT_TABLE();
};

#endif // TERMINAL_H
