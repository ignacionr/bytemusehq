#ifndef THEME_COMMANDS_H
#define THEME_COMMANDS_H

#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "theme.h"
#include <wx/choicdlg.h>

namespace ThemeCommands {

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
        "theme.select", "Select Color Theme", "",
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

    registry.Register(makeCommand(
        "theme.dark", "Use Dark Theme", "",
        "Switch to the dark color theme",
        [](CommandContext& ctx) {
            ThemeManager::Instance().SetCurrentTheme("dark");
        },
        [](const CommandContext& ctx) {
            return ThemeManager::Instance().GetCurrentTheme()->id != "dark";
        }
    ));

    registry.Register(makeCommand(
        "theme.light", "Use Light Theme", "",
        "Switch to the light color theme",
        [](CommandContext& ctx) {
            ThemeManager::Instance().SetCurrentTheme("light");
        },
        [](const CommandContext& ctx) {
            return ThemeManager::Instance().GetCurrentTheme()->id != "light";
        }
    ));

    registry.Register(makeCommand(
        "theme.toggle", "Toggle Dark/Light Theme", "",
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
}

} // namespace ThemeCommands

#endif // THEME_COMMANDS_H
