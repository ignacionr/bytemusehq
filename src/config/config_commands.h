#ifndef CONFIG_COMMANDS_H
#define CONFIG_COMMANDS_H

#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "config.h"
#include "../theme/theme.h"
#include <wx/msgdlg.h>

// Forward declaration
class MainFrame;

namespace ConfigCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "Preferences");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "config.openSettings", "Open Settings File", "",
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

    registry.Register(makeCommand(
        "config.reload", "Reload Settings", "",
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
}

} // namespace ConfigCommands

#endif // CONFIG_COMMANDS_H
