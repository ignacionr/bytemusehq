#ifndef VIEW_COMMANDS_H
#define VIEW_COMMANDS_H

#include "command.h"
#include "command_registry.h"
#include <wx/stc/stc.h>

namespace ViewCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "View");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "view.zoomIn", "Zoom In", "Ctrl++",
        "Increase editor font size",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ZoomIn();
            }
        }
    ));

    registry.Register(makeCommand(
        "view.zoomOut", "Zoom Out", "Ctrl+-",
        "Decrease editor font size",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->ZoomOut();
            }
        }
    ));

    registry.Register(makeCommand(
        "view.zoomReset", "Reset Zoom", "Ctrl+0",
        "Reset editor font size to default",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->SetZoom(0);
            }
        }
    ));

    registry.Register(makeCommand(
        "view.wordWrap", "Toggle Word Wrap", "",
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

    registry.Register(makeCommand(
        "view.lineNumbers", "Toggle Line Numbers", "",
        "Show or hide line numbers",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                int currentWidth = editor->GetMarginWidth(0);
                editor->SetMarginWidth(0, currentWidth > 0 ? 0 : 40);
            }
        }
    ));
}

} // namespace ViewCommands

#endif // VIEW_COMMANDS_H
