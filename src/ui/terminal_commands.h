#ifndef TERMINAL_COMMANDS_H
#define TERMINAL_COMMANDS_H

#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "terminal.h"

// Forward declaration
class MainFrame;

namespace TerminalCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "Terminal");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "terminal.toggle", "Toggle Terminal", "Ctrl+`",
        "Show or hide the integrated terminal",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (frame) {
                frame->ToggleTerminal();
            }
        }
    ));

    registry.Register(makeCommand(
        "terminal.show", "Show Terminal", "",
        "Show the integrated terminal",
        [](CommandContext& ctx) {
            auto* frame = static_cast<MainFrame*>(ctx.Get<wxWindow>("mainFrame"));
            if (frame) {
                frame->ShowTerminal(true);
            }
        }
    ));

    registry.Register(makeCommand(
        "terminal.hide", "Hide Terminal", "",
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

    registry.Register(makeCommand(
        "terminal.clear", "Clear Terminal", "",
        "Clear the terminal output",
        [](CommandContext& ctx) {
            auto* terminal = ctx.Get<Terminal>("terminal");
            if (terminal) {
                terminal->Clear();
            }
        }
    ));

    registry.Register(makeCommand(
        "terminal.focus", "Focus Terminal", "",
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
}

} // namespace TerminalCommands

#endif // TERMINAL_COMMANDS_H
