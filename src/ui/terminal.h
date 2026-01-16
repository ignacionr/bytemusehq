#ifndef TERMINAL_H
#define TERMINAL_H

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <memory>

/**
 * Terminal component for ByteMuseHQ.
 * Provides a simple command-line interface with a persistent shell session.
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

private:
    wxTextCtrl* m_output;       // Output display
    wxTextCtrl* m_input;        // Command input
    wxString m_workingDir;      // Current working directory
    
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
