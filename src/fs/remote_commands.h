#ifndef REMOTE_COMMANDS_H
#define REMOTE_COMMANDS_H

#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "../config/config.h"
#include "../ui/remote_folder_dialog.h"
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

// Forward declaration
class MainFrame;

namespace RemoteCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "Remote");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "remote.connect", "Connect to SSH Remote", "Ctrl+Shift+R",
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

    registry.Register(makeCommand(
        "remote.disconnect", "Disconnect from SSH Remote", "Ctrl+Shift+D",
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
        },
        [](const CommandContext& ctx) {
            // Enable when currently connected
            auto& config = Config::Instance();
            return config.GetBool("ssh.enabled", false);
        }
    ));

    registry.Register(makeCommand(
        "remote.reconnect", "Reconnect to SSH Remote", "",
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

    registry.Register(makeCommand(
        "remote.configureSSH", "Configure SSH Settings", "",
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

} // namespace RemoteCommands

#endif // REMOTE_COMMANDS_H
