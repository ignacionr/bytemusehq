#ifndef FILE_COMMANDS_H
#define FILE_COMMANDS_H

#include "command.h"
#include "command_registry.h"
#include "../config/config.h"
#include "../ui/remote_folder_dialog.h"
#include <wx/stc/stc.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/file.h>

// Forward declaration
class MainFrame;

namespace FileCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "File");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "file.new", "New File", "Ctrl+N",
        "Create a new empty file",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ClearAll();
                editor->EmptyUndoBuffer();
            }
        }
    ));

    registry.Register(makeCommand(
        "file.open", "Open File...", "Ctrl+O",
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

    registry.Register(makeCommand(
        "file.openFolder", "Open Folder...", "Ctrl+Shift+O",
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

    registry.Register(makeCommand(
        "file.save", "Save", "Ctrl+S",
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

    registry.Register(makeCommand(
        "file.saveAs", "Save As...", "Ctrl+Shift+S",
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
}

} // namespace FileCommands

#endif // FILE_COMMANDS_H
