#include "terminal.h"
#include <wx/filename.h>

wxBEGIN_EVENT_TABLE(Terminal, wxPanel)
    EVT_END_PROCESS(wxID_ANY, Terminal::OnProcessTerminated)
    EVT_IDLE(Terminal::OnIdle)
wxEND_EVENT_TABLE()

Terminal::Terminal(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
    , m_output(nullptr)
    , m_input(nullptr)
    , m_process(nullptr)
    , m_pid(0)
    , m_processInput(nullptr)
    , m_processOutput(nullptr)
    , m_processError(nullptr)
    , m_historyIndex(-1)
{
    m_workingDir = wxGetCwd();
    SetupUI();
    StartShell();
}

Terminal::~Terminal()
{
    StopShell();
}

void Terminal::SetupUI()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    // Header with label
    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* label = new wxStaticText(this, wxID_ANY, "Terminal");
    wxFont boldFont = label->GetFont();
    boldFont.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(boldFont);
    headerSizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
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
    m_output->SetBackgroundColour(wxColour(30, 30, 30));
    m_output->SetForegroundColour(wxColour(204, 204, 204));
    
    sizer->Add(m_output, 1, wxEXPAND | wxLEFT | wxRIGHT, 2);
    
    // Input area with prompt
    wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
    
    wxStaticText* prompt = new wxStaticText(this, wxID_ANY, ">");
    prompt->SetFont(monoFont);
    inputSizer->Add(prompt, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    
    m_input = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_input->SetFont(monoFont);
    m_input->Bind(wxEVT_TEXT_ENTER, &Terminal::OnInputEnter, this);
    m_input->Bind(wxEVT_KEY_DOWN, &Terminal::OnInputKeyDown, this);
    
    inputSizer->Add(m_input, 1, wxEXPAND | wxALL, 3);
    
    sizer->Add(inputSizer, 0, wxEXPAND | wxBOTTOM, 3);
    
    SetSizer(sizer);
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

void Terminal::StartShell()
{
    if (m_process) {
        StopShell();
    }
    
    m_process = new wxProcess(this);
    m_process->Redirect();
    
    wxString shell = GetShellCommand();
    
    // Set environment to be interactive-ish but not fully
    wxExecuteEnv env;
    env.cwd = m_workingDir;
    
#ifdef __WXMSW__
    m_pid = wxExecute(shell, wxEXEC_ASYNC, m_process, &env);
#else
    // On Unix, don't use -i flag as it tries to use TTY which we don't have
    // Use a simpler approach that still processes .profile/.bashrc
    m_pid = wxExecute(shell, wxEXEC_ASYNC, m_process, &env);
#endif
    
    if (m_pid == 0) {
        m_output->AppendText("Failed to start shell: " + shell + "\n");
        delete m_process;
        m_process = nullptr;
        return;
    }
    
    m_processInput = m_process->GetOutputStream();
    m_processOutput = m_process->GetInputStream();
    m_processError = m_process->GetErrorStream();
    
    m_output->AppendText("Shell started: " + shell + "\n");
    m_output->AppendText("Working directory: " + m_workingDir + "\n\n");
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
