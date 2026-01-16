#ifndef COMMAND_PALETTE_H
#define COMMAND_PALETTE_H

#include "command.h"
#include "command_registry.h"
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/srchctrl.h>

/**
 * A VS Code-style command palette dialog.
 * Provides fuzzy search and keyboard navigation for commands.
 */
class CommandPalette : public wxDialog {
public:
    CommandPalette(wxWindow* parent, CommandContext& context);

    // Get the selected command (after dialog closes with OK)
    CommandPtr GetSelectedCommand() const { return m_selectedCommand; }

private:
    void CreateUI();
    void UpdateCommandList();
    void SelectCommand(int index);
    void ExecuteSelected();

    // Event handlers
    void OnSearchTextChanged(wxCommandEvent& event);
    void OnListItemActivated(wxCommandEvent& event);
    void OnListItemSelected(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    wxSearchCtrl* m_searchCtrl;
    wxListBox* m_listBox;
    CommandContext& m_context;
    std::vector<CommandPtr> m_filteredCommands;
    CommandPtr m_selectedCommand;

    wxDECLARE_EVENT_TABLE();
};

#endif // COMMAND_PALETTE_H
