#include "terminal.h"
#include "../config/config.h"
#include <wx/filename.h>

wxBEGIN_EVENT_TABLE(Terminal, wxPanel)
    EVT_END_PROCESS(wxID_ANY, Terminal::OnProcessTerminated)
    EVT_IDLE(Terminal::OnIdle)
wxEND_EVENT_TABLE()

/**
 * Load SSH configuration from the global config.
 */
static SshConfig LoadSshConfigFromSettings() {
    auto& config = Config::Instance();
    SshConfig ssh;
    
    ssh.enabled = config.GetBool("ssh.enabled", false);
    ssh.host = config.GetString("ssh.host", "");
    ssh.port = config.GetInt("ssh.port", 22);
    ssh.user = config.GetString("ssh.user", "");
    ssh.identityFile = config.GetString("ssh.identityFile", "");
    ssh.remotePath = config.GetString("ssh.remotePath", "~");
    ssh.extraOptions = config.GetString("ssh.extraOptions", "");
    ssh.forwardAgent = config.GetBool("ssh.forwardAgent", false);
    ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
    
    return ssh;
}

Terminal::Terminal(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
    , m_output(nullptr)
    , m_input(nullptr)
    , m_label(nullptr)
    , m_prompt(nullptr)
    , m_process(nullptr)
    , m_pid(0)
    , m_processInput(nullptr)
    , m_processOutput(nullptr)
    , m_processError(nullptr)
    , m_historyIndex(-1)
    , m_themeListenerId(0)
{
    m_workingDir = wxGetCwd();
    
    // Load SSH configuration from settings
    m_sshConfig = LoadSshConfigFromSettings();
    
    SetupUI();
    ApplyCurrentTheme();
    StartShell();
    
    // Listen for theme changes
    m_themeListenerId = ThemeManager::Instance().AddChangeListener(
        [this](const ThemePtr& theme) {
            ApplyTheme(theme);
        });
}

Terminal::~Terminal()
{
    if (m_themeListenerId > 0) {
        ThemeManager::Instance().RemoveChangeListener(m_themeListenerId);
    }
    StopShell();
}

void Terminal::SetupUI()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Header with label
    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    m_label = new wxStaticText(this, wxID_ANY, "Terminal");
    wxFont boldFont = m_label->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_label->SetFont(boldFont);
    headerSizer->Add(m_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    headerSizer->AddStretchSpacer();
    
    // Clear button
    wxButton* clearBtn = new wxButton(this, wxID_ANY, "Clear", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    clearBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Clear(); });
    headerSizer->Add(clearBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    
    sizer->Add(headerSizer, 0, wxEXPAND | wxTOP | wxBOTTOM, 3);
    
    // Output area
    m_output = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);
    
    // Use monospace font
    wxFont monoFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    m_output->SetFont(monoFont);
    
    sizer->Add(m_output, 1, wxEXPAND | wxLEFT | wxRIGHT, 2);
    
    // Input area with prompt
    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
    
    m_prompt = new wxStaticText(this, wxID_ANY, ">");
    m_prompt->SetFont(monoFont);
    inputSizer->Add(m_prompt, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    
    m_input = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_input->SetFont(monoFont);
    m_input->Bind(wxEVT_TEXT_ENTER, &Terminal::OnInputEnter, this);
    m_input->Bind(wxEVT_KEY_DOWN, &Terminal::OnInputKeyDown, this);
    
    inputSizer->Add(m_input, 1, wxEXPAND | wxALL, 3);
    
    sizer->Add(inputSizer, 0, wxEXPAND | wxBOTTOM, 3);
    
    SetSizer(sizer);
}

void Terminal::ApplyCurrentTheme()
{
    auto theme = ThemeManager::Instance().GetCurrentTheme();
    if (theme) {
        ApplyTheme(theme);
    }
}

void Terminal::ApplyTheme(const ThemePtr& theme)
{
    if (!theme) return;
    
    const auto& colors = theme->terminal;
    
    // Panel background
    SetBackgroundColour(colors.background);
    
    // Output area
    if (m_output) {
        m_output->SetBackgroundColour(colors.background);
        m_output->SetForegroundColour(colors.foreground);
    }
    
    // Input area
    if (m_input) {
        m_input->SetBackgroundColour(colors.inputBackground);
        m_input->SetForegroundColour(colors.inputForeground);
    }
    
    // Label and prompt
    if (m_label) {
        m_label->SetForegroundColour(colors.foreground);
    }
    if (m_prompt) {
        m_prompt->SetForegroundColour(colors.prompt);
    }
    
    // Refresh to apply colors
    Refresh();
}

wxString Terminal::GetShellCommand()
{
#ifdef __WXMSW__
    // Windows: prefer PowerShell, fall back to cmd.exe
    return "cmd.exe";
#elif defined(__WXOSX__)
    // macOS: prefer zsh (default since Catalina)
    return "/bin/zsh";
#else
    // Linux and others: use bash
    return "/bin/bash";
#endif
}

void Terminal::SetSshConfig(const SshConfig& config)
{
    m_sshConfig = config;
}

void Terminal::Reconnect()
{
    // Reload SSH config from settings
    m_sshConfig = LoadSshConfigFromSettings();
    StopShell();
    StartShell();
}

void Terminal::StartShell()
{
    if (m_process) {
        StopShell();
    }
    
    m_process = new wxProcess(this);
    m_process->Redirect();
    
    wxString shell;
    wxString displayMessage;
    
    // Check if we should use SSH
    if (m_sshConfig.IsValid()) {
        // Build SSH command
        shell = m_sshConfig.BuildSshCommand();
        displayMessage = wxString::Format("Connecting to %s@%s:%d...\n",
            m_sshConfig.user.IsEmpty() ? wxGetUserId() : m_sshConfig.user,
            m_sshConfig.host, m_sshConfig.port);
    } else {
        // Local shell
        shell = GetShellCommand();
        displayMessage = "Shell started: " + shell + "\n";
    }
    
    // Set environment
    wxExecuteEnv env;
    env.cwd = m_workingDir;
    
    m_pid = wxExecute(shell, wxEXEC_ASYNC, m_process, &env);
    
    if (m_pid == 0) {
        m_output->AppendText("Failed to start: " + shell + "\n");
        delete m_process;
        m_process = nullptr;
        return;
    }
    
    m_processInput = m_process->GetOutputStream();
    m_processOutput = m_process->GetInputStream();
    m_processError = m_process->GetErrorStream();
    
    m_output->AppendText(displayMessage);
    
    // For SSH, change to the remote working directory
    if (m_sshConfig.IsValid() && !m_sshConfig.remotePath.IsEmpty()) {
        m_output->AppendText("Remote directory: " + m_sshConfig.remotePath + "\n\n");
        // Send cd command after a brief delay for connection establishment
        wxString cdCmd = "cd " + m_sshConfig.remotePath + "\n";
        m_processInput->Write(cdCmd.c_str(), cdCmd.length());
    } else {
        m_output->AppendText("Working directory: " + m_workingDir + "\n\n");
    }
}

void Terminal::StopShell()
{
    if (m_process && m_pid > 0) {
        // Send exit command gracefully
        if (m_processInput && m_processInput->IsOk()) {
            wxString exitCmd = "exit\n";
            m_processInput->Write(exitCmd.c_str(), exitCmd.length());
        }
        
        // Give it a moment, then kill if needed
        wxMilliSleep(100);
        
        if (wxProcess::Exists(m_pid)) {
            wxProcess::Kill(m_pid, wxSIGTERM);
        }
    }
    
    delete m_process;
    m_process = nullptr;
    m_pid = 0;
    m_processInput = nullptr;
    m_processOutput = nullptr;
    m_processError = nullptr;
}

void Terminal::ExecuteCommand(const wxString& command)
{
    if (!m_process || !m_processInput || !m_processInput->IsOk()) {
        m_output->AppendText("Shell not running. Restarting...\n");
        StartShell();
        if (!m_process) return;
    }
    
    // Add to history if not empty and not duplicate
    if (!command.IsEmpty()) {
        if (m_history.IsEmpty() || m_history.Last() != command) {
            m_history.Add(command);
        }
        m_historyIndex = m_history.GetCount();
    }
    
    // Echo the command
    m_output->SetDefaultStyle(wxTextAttr(wxColour(100, 200, 100)));
    m_output->AppendText("> " + command + "\n");
    m_output->SetDefaultStyle(wxTextAttr(wxColour(204, 204, 204)));
    
    // Send to shell
    wxString cmdLine = command + "\n";
    m_processInput->Write(cmdLine.c_str(), cmdLine.length());
    
    // Scroll to end
    m_output->ShowPosition(m_output->GetLastPosition());
}

void Terminal::ReadProcessOutput()
{
    if (!m_process) return;
    
    bool hasOutput = false;
    
    // Read stdout
    if (m_processOutput && m_processOutput->CanRead()) {
        char buffer[4096];
        while (m_processOutput->CanRead()) {
            m_processOutput->Read(buffer, sizeof(buffer) - 1);
            size_t count = m_processOutput->LastRead();
            if (count > 0) {
                buffer[count] = '\0';
                m_output->AppendText(wxString::FromUTF8(buffer, count));
                hasOutput = true;
            } else {
                break;
            }
        }
    }
    
    // Read stderr
    if (m_processError && m_processError->CanRead()) {
        char buffer[4096];
        while (m_processError->CanRead()) {
            m_processError->Read(buffer, sizeof(buffer) - 1);
            size_t count = m_processError->LastRead();
            if (count > 0) {
                buffer[count] = '\0';
                // Show errors in red
                m_output->SetDefaultStyle(wxTextAttr(wxColour(255, 100, 100)));
                m_output->AppendText(wxString::FromUTF8(buffer, count));
                m_output->SetDefaultStyle(wxTextAttr(wxColour(204, 204, 204)));
                hasOutput = true;
            } else {
                break;
            }
        }
    }
    
    if (hasOutput) {
        m_output->ShowPosition(m_output->GetLastPosition());
    }
}

void Terminal::Clear()
{
    m_output->Clear();
}

void Terminal::SetWorkingDirectory(const wxString& dir)
{
    m_workingDir = dir;
    if (m_process && m_processInput && m_processInput->IsOk()) {
        // Change directory in the shell
        ExecuteCommand("cd \"" + dir + "\"");
    }
}

void Terminal::OnInputEnter(wxCommandEvent& event)
{
    wxString command = m_input->GetValue();
    m_input->Clear();
    ExecuteCommand(command);
}

void Terminal::OnInputKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    
    if (keyCode == WXK_UP) {
        // Previous command in history
        if (!m_history.IsEmpty() && m_historyIndex > 0) {
            m_historyIndex--;
            m_input->SetValue(m_history[m_historyIndex]);
            m_input->SetInsertionPointEnd();
        }
    }
    else if (keyCode == WXK_DOWN) {
        // Next command in history
        if (!m_history.IsEmpty()) {
            if (m_historyIndex < (int)m_history.GetCount() - 1) {
                m_historyIndex++;
                m_input->SetValue(m_history[m_historyIndex]);
                m_input->SetInsertionPointEnd();
            }
            else {
                m_historyIndex = m_history.GetCount();
                m_input->Clear();
            }
        }
    }
    else if (keyCode == WXK_ESCAPE) {
        m_input->Clear();
        m_historyIndex = m_history.GetCount();
    }
    else {
        event.Skip();
    }
}

void Terminal::OnIdle(wxIdleEvent& event)
{
    ReadProcessOutput();
    
    // Request more idle events if shell is running
    if (m_process) {
        event.RequestMore();
    }
}

void Terminal::OnProcessTerminated(wxProcessEvent& event)
{
    ReadProcessOutput(); // Get any remaining output
    
    m_output->AppendText("\n[Shell process terminated with exit code: " + 
                         wxString::Format("%d", event.GetExitCode()) + "]\n");
    
    delete m_process;
    m_process = nullptr;
    m_pid = 0;
    m_processInput = nullptr;
    m_processOutput = nullptr;
    m_processError = nullptr;
}
