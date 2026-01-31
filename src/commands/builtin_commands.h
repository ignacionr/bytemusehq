#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

#include "command.h"
#include "command_registry.h"
#include "../ui/terminal.h"
#include "../ui/remote_folder_dialog.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>
#include <wx/aboutdlg.h>
#include <wx/choicdlg.h>

// Forward declaration to avoid circular include
class MainFrame;

/**
 * Factory for creating built-in commands.
 * Add new commands here or create separate files for command groups.
 */
namespace BuiltinCommands {

// Helper to create and configure a command
inline CommandPtr MakeCommand(const wxString& id, const wxString& title, 
                              const wxString& category, 
                              const wxString& shortcut,
                              const wxString& description,
                              Command::ExecuteFunc execFunc,
                              Command::EnabledFunc enabledFunc = nullptr) {
    auto cmd = std::make_shared<Command>(id, title, category);
    cmd->SetShortcut(shortcut);
    cmd->SetDescription(description);
    cmd->SetExecuteHandler(std::move(execFunc));
    if (enabledFunc) {
        cmd->SetEnabledHandler(std::move(enabledFunc));
    }
    return cmd;
}

// Register all built-in commands
inline void RegisterAll() {
    auto& registry = CommandRegistry::Instance();

    // ========== File Commands ==========
    
    registry.Register(MakeCommand(
        "file.new", "New File", "File", "Ctrl+N",
        "Create a new empty file",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ClearAll();
                editor->EmptyUndoBuffer();
            }
        }
    ));

    registry.Register(MakeCommand(
        "file.open", "Open File...", "File", "Ctrl+O",
        "Open an existing file",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (!window || !editor) return;

            wxFileDialog dlg(window, "Open File", "", "",
                            "All files (*.*)|*.*|"
                            "C++ files (*.cpp;*.h;*.hpp)|*.cpp;*.h;*.hpp|"
                            "Text files (*.txt)|*.txt",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

            if (dlg.ShowModal() == wxID_OK) {
                wxString path = dlg.GetPath();
                wxFile file(path);
                if (file.IsOpened()) {
                    wxString content;
                    file.ReadAll(&content);
                    editor->SetText(content);
                    editor->EmptyUndoBuffer();
                }
            }
        }
    ));

    registry.Register(MakeCommand(
        "file.openFolder", "Open Folder...", "File", "Ctrl+Shift+O",
        "Open a folder in the file tree",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (!window || !frame) return;

            // Check if SSH is enabled
            auto& config = Config::Instance();
            bool sshEnabled = config.GetBool("ssh.enabled", false);
            std::string sshHost = config.GetString("ssh.host", "").ToStdString();
            
            if (sshEnabled && !sshHost.empty()) {
                // Use remote folder dialog for SSH
                UI::RemoteFolderSshConfig sshConfig;
                sshConfig.host = sshHost;
                sshConfig.port = config.GetInt("ssh.port", 22);
                sshConfig.user = config.GetString("ssh.user", "").ToStdString();
                sshConfig.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
                sshConfig.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
                sshConfig.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
                
                wxString initialPath = config.GetString("ssh.remotePath", "~");
                UI::RemoteFolderDialog dlg(window, sshConfig, initialPath);
                
                if (dlg.ShowModal() == wxID_OK) {
                    wxString remotePath = dlg.GetPath();
                    // Update ssh.remotePath config and refresh
                    config.Set("ssh.remotePath", remotePath);
                    frame->OpenFolder(remotePath, true /* isRemote */);
                }
            } else {
                // Use local folder dialog
                wxDirDialog dlg(window, "Open Folder", wxGetCwd(),
                               wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

                if (dlg.ShowModal() == wxID_OK) {
                    frame->OpenFolder(dlg.GetPath());
                }
            }
        }
    ));

    registry.Register(MakeCommand(
        "file.save", "Save", "File", "Ctrl+S",
        "Save the current file",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            auto* currentFile = ctx.Get<wxString>("currentFile");
            
            if (!window || !editor) return;

            wxString path;
            if (currentFile && !currentFile->IsEmpty()) {
                path = *currentFile;
            } else {
                wxFileDialog dlg(window, "Save File", "", "",
                                "All files (*.*)|*.*",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
                if (dlg.ShowModal() == wxID_OK) {
                    path = dlg.GetPath();
                } else {
                    return;
                }
            }

            wxFile file(path, wxFile::write);
            if (file.IsOpened()) {
                file.Write(editor->GetText());
            }
        }
    ));

    registry.Register(MakeCommand(
        "file.saveAs", "Save As...", "File", "Ctrl+Shift+S",
        "Save the current file with a new name",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (!window || !editor) return;

            wxFileDialog dlg(window, "Save File As", "", "",
                            "All files (*.*)|*.*",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

            if (dlg.ShowModal() == wxID_OK) {
                wxString path = dlg.GetPath();
                wxFile file(path, wxFile::write);
                if (file.IsOpened()) {
                    file.Write(editor->GetText());
                }
            }
        }
    ));

    // ========== Edit Commands ==========

    registry.Register(MakeCommand(
        "edit.undo", "Undo", "Edit", "Ctrl+Z",
        "Undo the last action",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor && editor->CanUndo()) {
                editor->Undo();
            }
        },
        [](const CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            return editor && editor->CanUndo();
        }
    ));

    registry.Register(MakeCommand(
        "edit.redo", "Redo", "Edit", "Ctrl+Y",
        "Redo the last undone action",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor && editor->CanRedo()) {
                editor->Redo();
            }
        },
        [](const CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            return editor && editor->CanRedo();
        }
    ));

    registry.Register(MakeCommand(
        "edit.cut", "Cut", "Edit", "Ctrl+X",
        "Cut the selected text",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Cut();
            }
        }
    ));

    registry.Register(MakeCommand(
        "edit.copy", "Copy", "Edit", "Ctrl+C",
        "Copy the selected text",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Copy();
            }
        }
    ));

    registry.Register(MakeCommand(
        "edit.paste", "Paste", "Edit", "Ctrl+V",
        "Paste from clipboard",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Paste();
            }
        }
    ));

    registry.Register(MakeCommand(
        "edit.selectAll", "Select All", "Edit", "Ctrl+A",
        "Select all text in the editor",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->SelectAll();
            }
        }
    ));

    registry.Register(MakeCommand(
        "edit.find", "Find...", "Edit", "Ctrl+F",
        "Find text in the editor",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (!window || !editor) return;

            wxTextEntryDialog dlg(window, "Find:", "Find");
            if (dlg.ShowModal() == wxID_OK) {
                wxString searchText = dlg.GetValue();
                int pos = editor->FindText(editor->GetCurrentPos(), 
                                          editor->GetLength(),
                                          searchText);
                if (pos != wxNOT_FOUND) {
                    editor->GotoPos(pos);
                    editor->SetSelection(pos, pos + searchText.Length());
                } else {
                    wxMessageBox("Text not found.", "Find", wxOK | wxICON_INFORMATION);
                }
            }
        }
    ));

    registry.Register(MakeCommand(
        "edit.goToLine", "Go to Line...", "Edit", "Ctrl+G",
        "Jump to a specific line number",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (!window || !editor) return;

            int lineCount = editor->GetLineCount();
            wxTextEntryDialog dlg(window, 
                wxString::Format("Enter line number (1-%d):", lineCount),
                "Go to Line");

            if (dlg.ShowModal() == wxID_OK) {
                long lineNum;
                if (dlg.GetValue().ToLong(&lineNum) && lineNum >= 1 && lineNum <= lineCount) {
                    editor->GotoLine(lineNum - 1);  // 0-indexed
                }
            }
        }
    ));

    // ========== View Commands ==========

    registry.Register(MakeCommand(
        "view.zoomIn", "Zoom In", "View", "Ctrl++",
        "Increase editor font size",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ZoomIn();
            }
        }
    ));

    registry.Register(MakeCommand(
        "view.zoomOut", "Zoom Out", "View", "Ctrl+-",
        "Decrease editor font size",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ZoomOut();
            }
        }
    ));

    registry.Register(MakeCommand(
        "view.zoomReset", "Reset Zoom", "View", "Ctrl+0",
        "Reset editor font size to default",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->SetZoom(0);
            }
        }
    ));

    registry.Register(MakeCommand(
        "view.wordWrap", "Toggle Word Wrap", "View", "",
        "Toggle word wrapping in the editor",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                int currentMode = editor->GetWrapMode();
                editor->SetWrapMode(currentMode == wxSTC_WRAP_NONE ? 
                                   wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
            }
        }
    ));

    registry.Register(MakeCommand(
        "view.lineNumbers", "Toggle Line Numbers", "View", "",
        "Show or hide line numbers",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                int currentWidth = editor->GetMarginWidth(0);
                editor->SetMarginWidth(0, currentWidth > 0 ? 0 : 40);
            }
        }
    ));

    // ========== Terminal Commands ==========

    registry.Register(MakeCommand(
        "terminal.toggle", "Toggle Terminal", "Terminal", "Ctrl+`",
        "Show or hide the integrated terminal",
        [](CommandContext& ctx) {
            // MainFrame is stored in context - we use void* casting
            // This is safe because we know MainFrame sets itself
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (frame) {
                frame->ToggleTerminal();
            }
        }
    ));

    registry.Register(MakeCommand(
        "terminal.show", "Show Terminal", "Terminal", "",
        "Show the integrated terminal",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (frame) {
                frame->ShowTerminal(true);
            }
        }
    ));

    registry.Register(MakeCommand(
        "terminal.hide", "Hide Terminal", "Terminal", "",
        "Hide the integrated terminal",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (frame) {
                frame->ShowTerminal(false);
            }
        },
        [](const CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            return frame && frame->IsTerminalVisible();
        }
    ));

    registry.Register(MakeCommand(
        "terminal.clear", "Clear Terminal", "Terminal", "",
        "Clear the terminal output",
        [](CommandContext& ctx) {
            auto* terminal = ctx.Get<Terminal>("terminal");
            if (terminal) {
                terminal->Clear();
            }
        }
    ));

    registry.Register(MakeCommand(
        "terminal.focus", "Focus Terminal", "Terminal", "",
        "Move focus to the terminal input",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            auto* terminal = ctx.Get<Terminal>("terminal");
            if (frame && terminal) {
                frame->ShowTerminal(true);
                terminal->SetFocus();
            }
        }
    ));

    // ========== Application Commands ==========

    registry.Register(MakeCommand(
        "app.commandPalette", "Command Palette", "Application", "Ctrl+Shift+P",
        "Open the command palette",
        [](CommandContext& ctx) {
            // This is handled specially by the main frame
            // It's here so it shows up in searches
        }
    ));

    registry.Register(MakeCommand(
        "app.about", "About ByteMuseHQ", "Help", "",
        "Show information about this application",
        [](CommandContext& ctx) {
            wxAboutDialogInfo info;
            info.SetName("ByteMuseHQ");
            info.SetVersion("1.0.0");
            info.SetDescription("A lightweight code editor built with wxWidgets.");
            info.SetCopyright("(C) 2024-2026 ByteMuse");
            wxAboutBox(info);
        }
    ));

    registry.Register(MakeCommand(
        "app.quit", "Quit", "Application", "Ctrl+Q",
        "Exit the application",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            if (window) {
                window->Close();
            }
        }
    ));

    registry.Register(MakeCommand(
        "app.toggleDebugLog", "Toggle Debug Log Window", "Developer", "Ctrl+Shift+L",
        "Show or hide the debug log window",
        [](CommandContext& ctx) {
            // Get the active log target (which should be our wxLogWindow)
            wxLogWindow* logWindow = dynamic_cast<wxLogWindow*>(wxLog::GetActiveTarget());
            if (logWindow) {
                wxFrame* logFrame = logWindow->GetFrame();
                if (logFrame) {
                    bool show = !logFrame->IsShown();
                    logFrame->Show(show);
                    if (show) {
                        logFrame->Raise();
                    }
                }
            }
        }
    ));

    // ========== Theme Commands ==========

    registry.Register(MakeCommand(
        "theme.select", "Select Color Theme", "Preferences", "",
        "Choose a color theme for the editor",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            if (!window) return;

            auto& themeManager = ThemeManager::Instance();
            auto themes = themeManager.GetAllThemes();
            
            wxArrayString themeNames;
            for (const auto& theme : themes) {
                themeNames.Add(theme->name + (theme->isDark ? " (Dark)" : " (Light)"));
            }
            
            // Find current selection
            int currentIdx = 0;
            auto currentTheme = themeManager.GetCurrentTheme();
            for (size_t i = 0; i < themes.size(); i++) {
                if (themes[i]->id == currentTheme->id) {
                    currentIdx = i;
                    break;
                }
            }
            
            wxSingleChoiceDialog dlg(window, "Select a color theme:",
                                     "Color Theme", themeNames);
            dlg.SetSelection(currentIdx);
            
            if (dlg.ShowModal() == wxID_OK) {
                int selection = dlg.GetSelection();
                if (selection >= 0 && selection < static_cast<int>(themes.size())) {
                    themeManager.SetCurrentTheme(themes[selection]->id);
                }
            }
        }
    ));

    registry.Register(MakeCommand(
        "theme.dark", "Use Dark Theme", "Preferences", "",
        "Switch to the dark color theme",
        [](CommandContext& ctx) {
            ThemeManager::Instance().SetCurrentTheme("dark");
        },
        [](const CommandContext& ctx) {
            return ThemeManager::Instance().GetCurrentTheme()->id != "dark";
        }
    ));

    registry.Register(MakeCommand(
        "theme.light", "Use Light Theme", "Preferences", "",
        "Switch to the light color theme",
        [](CommandContext& ctx) {
            ThemeManager::Instance().SetCurrentTheme("light");
        },
        [](const CommandContext& ctx) {
            return ThemeManager::Instance().GetCurrentTheme()->id != "light";
        }
    ));

    registry.Register(MakeCommand(
        "theme.toggle", "Toggle Dark/Light Theme", "Preferences", "",
        "Switch between dark and light themes",
        [](CommandContext& ctx) {
            auto& manager = ThemeManager::Instance();
            if (manager.GetCurrentTheme()->isDark) {
                manager.SetCurrentTheme("light");
            } else {
                manager.SetCurrentTheme("dark");
            }
        }
    ));

    // ========== Configuration Commands ==========

    registry.Register(MakeCommand(
        "config.openSettings", "Open Settings File", "Preferences", "",
        "Open the configuration file for editing",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (!frame) return;
            
            auto& config = Config::Instance();
            wxString configPath = config.GetConfigFilePath();
            
            // Ensure config file exists
            config.Save();
            
            // Open in editor
            auto* editor = frame->GetEditor();
            if (editor) {
                editor->OpenFile(configPath);
            }
        }
    ));

    registry.Register(MakeCommand(
        "config.reload", "Reload Settings", "Preferences", "",
        "Reload configuration from file",
        [](CommandContext& ctx) {
            auto& config = Config::Instance();
            if (config.Load()) {
                // Re-initialize theme from config
                ThemeManager::Instance().Initialize();
                wxMessageBox("Settings reloaded successfully.", "Settings", 
                            wxOK | wxICON_INFORMATION);
            } else {
                wxMessageBox("Failed to reload settings.", "Error", 
                            wxOK | wxICON_ERROR);
            }
        }
    ));

    // ========== Remote/SSH Commands ==========

    registry.Register(MakeCommand(
        "remote.connect", "Connect to SSH Remote", "Remote", "Ctrl+Shift+R",
        "Connect to a remote host via SSH",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (!window || !frame) return;
            
            auto& config = Config::Instance();
            
            // Get SSH settings or prompt for them
            wxString host = config.GetString("ssh.host", "");
            wxString user = config.GetString("ssh.user", "");
            
            // If no host configured, prompt for it
            if (host.IsEmpty()) {
                host = wxGetTextFromUser("Enter remote host:", "SSH Host", host, window);
                if (host.IsEmpty()) return;
            }
            
            if (user.IsEmpty()) {
                user = wxGetTextFromUser("Enter username (leave empty for default):", 
                                        "SSH User", user, window);
            }
            
            // Update config
            config.Set("ssh.enabled", true);
            config.Set("ssh.host", host);
            if (!user.IsEmpty()) {
                config.Set("ssh.user", user);
            }
            config.Save();
            
            // Get remote path to open
            wxString remotePath = config.GetString("ssh.remotePath", "~");
            
            // Use remote folder dialog to select path
            UI::RemoteFolderSshConfig sshConfig;
            sshConfig.host = host.ToStdString();
            sshConfig.port = config.GetInt("ssh.port", 22);
            sshConfig.user = user.ToStdString();
            sshConfig.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
            sshConfig.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
            sshConfig.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
            
            UI::RemoteFolderDialog dlg(window, sshConfig, remotePath);
            
            if (dlg.ShowModal() == wxID_OK) {
                remotePath = dlg.GetPath();
                config.Set("ssh.remotePath", remotePath);
                config.Save();
                
                // Open the remote folder
                frame->OpenFolder(remotePath, true);
                
                wxMessageBox(wxString::Format("Connected to %s", host), 
                           "SSH Connection", wxOK | wxICON_INFORMATION);
            }
        },
        [](const CommandContext& ctx) {
            // Enable when not connected or when SSH is disabled
            auto& config = Config::Instance();
            return !config.GetBool("ssh.enabled", false);
        }
    ));

    registry.Register(MakeCommand(
        "remote.disconnect", "Disconnect from SSH Remote", "Remote", "Ctrl+Shift+D",
        "Disconnect from the current SSH remote and switch to local mode",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (!frame) return;
            
            auto& config = Config::Instance();
            
            // Disable SSH
            config.Set("ssh.enabled", false);
            config.Save();
            
            // Open local folder (current working directory or home)
            wxString localPath = wxGetCwd();
            if (localPath.IsEmpty()) {
                localPath = wxGetHomeDir();
            }
            
            frame->OpenFolder(localPath, false);
            
            wxMessageBox("Disconnected from remote. Switched to local mode.", 
                       "SSH Disconnected", wxOK | wxICON_INFORMATION);
        },
        [](const CommandContext& ctx) {
            // Enable when currently connected
            auto& config = Config::Instance();
            return config.GetBool("ssh.enabled", false);
        }
    ));

    registry.Register(MakeCommand(
        "remote.reconnect", "Reconnect to SSH Remote", "Remote", "",
        "Reconnect to the configured SSH remote",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (!frame) return;
            
            auto& config = Config::Instance();
            wxString host = config.GetString("ssh.host", "");
            
            if (host.IsEmpty()) {
                wxMessageBox("No SSH host configured. Use 'Connect to SSH Remote' first.", 
                           "Error", wxOK | wxICON_ERROR);
                return;
            }
            
            // Enable SSH and re-open the remote folder
            config.Set("ssh.enabled", true);
            config.Save();
            
            wxString remotePath = config.GetString("ssh.remotePath", "~");
            frame->OpenFolder(remotePath, true);
            
            wxMessageBox(wxString::Format("Reconnected to %s", host), 
                       "SSH Connection", wxOK | wxICON_INFORMATION);
        },
        [](const CommandContext& ctx) {
            // Enable when not currently connected but host is configured
            auto& config = Config::Instance();
            return !config.GetBool("ssh.enabled", false) && 
                   !config.GetString("ssh.host", "").IsEmpty();
        }
    ));

    registry.Register(MakeCommand(
        "remote.configureSSH", "Configure SSH Settings", "Remote", "",
        "Configure SSH connection settings",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            if (!window) return;
            
            auto& config = Config::Instance();
            
            // Simple dialog to configure SSH settings
            wxString msg = "Configure SSH settings in the config file:\n\n"
                          "ssh.host - Remote hostname or IP\n"
                          "ssh.port - SSH port (default: 22)\n"
                          "ssh.user - Username for SSH connection\n"
                          "ssh.identityFile - Path to SSH key file\n"
                          "ssh.remotePath - Default remote directory\n\n"
                          "Open settings file now?";
            
            int result = wxMessageBox(msg, "SSH Configuration", 
                                     wxYES_NO | wxICON_QUESTION, window);
            
            if (result == wxYES) {
                auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
                if (frame) {
                    wxString configPath = config.GetConfigFilePath();
                    config.Save();
                    auto* editor = frame->GetEditor();
                    if (editor) {
                        editor->OpenFile(configPath);
                    }
                }
            }
        }
    ));
}

} // namespace BuiltinCommands

#endif // BUILTIN_COMMANDS_H
