#ifndef EDIT_COMMANDS_H
#define EDIT_COMMANDS_H

#include "command.h"
#include "command_registry.h"
#include <wx/stc/stc.h>
#include <wx/textdlg.h>
#include <wx/msgdlg.h>

namespace EditCommands {

inline void Register() {
    auto& registry = CommandRegistry::Instance();

    // Helper to create commands
    auto makeCommand = [](const wxString& id, const wxString& title, 
                          const wxString& shortcut, const wxString& description,
                          Command::ExecuteFunc execFunc,
                          Command::EnabledFunc enabledFunc = nullptr) {
        auto cmd = std::make_shared<Command>(id, title, "Edit");
        cmd->SetShortcut(shortcut);
        cmd->SetDescription(description);
        cmd->SetExecuteHandler(std::move(execFunc));
        if (enabledFunc) {
            cmd->SetEnabledHandler(std::move(enabledFunc));
        }
        return cmd;
    };

    registry.Register(makeCommand(
        "edit.undo", "Undo", "Ctrl+Z",
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

    registry.Register(makeCommand(
        "edit.redo", "Redo", "Ctrl+Y",
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

    registry.Register(makeCommand(
        "edit.cut", "Cut", "Ctrl+X",
        "Cut the selected text",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Cut();
            }
        }
    ));

    registry.Register(makeCommand(
        "edit.copy", "Copy", "Ctrl+C",
        "Copy the selected text",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Copy();
            }
        }
    ));

    registry.Register(makeCommand(
        "edit.paste", "Paste", "Ctrl+V",
        "Paste from clipboard",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->Paste();
            }
        }
    ));

    registry.Register(makeCommand(
        "edit.selectAll", "Select All", "Ctrl+A",
        "Select all text in the editor",
        [](CommandContext& ctx) {
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                editor->SelectAll();
            }
        }
    ));

    registry.Register(makeCommand(
        "edit.find", "Find...", "Ctrl+F",
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

    registry.Register(makeCommand(
        "edit.goToLine", "Go to Line...", "Ctrl+G",
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
}

} // namespace EditCommands

#endif // EDIT_COMMANDS_H
