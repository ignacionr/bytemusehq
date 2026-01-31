#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include "command.h"
#include "command_registry.h"
#include <wx/aboutdlg.h>
#include <wx/log.h>

namespace AppCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& category, const wxString& shortcut, 
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
    };

    registry.Register(makeCommand(
        "app.commandPalette", "Command Palette", "Application", "Ctrl+Shift+P",
        "Open the command palette",
        [](CommandContext& ctx) {
            // This is handled specially by the main frame
            // It's here so it shows up in searches
        }
    ));

    registry.Register(makeCommand(
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

    registry.Register(makeCommand(
        "app.quit", "Quit", "Application", "Ctrl+Q",
        "Exit the application",
        [](CommandContext& ctx) {
            auto* window = ctx.Get<wxWindow>("window");
            if (window) {
                window->Close();
            }
        }
    ));

    registry.Register(makeCommand(
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
}

} // namespace AppCommands

#endif // APP_COMMANDS_H
