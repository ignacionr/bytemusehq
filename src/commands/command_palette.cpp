#include "command_palette.h"
#include <wx/settings.h>

enum {
    ID_SEARCH_CTRL = wxID_HIGHEST + 100,
    ID_COMMAND_LIST
};

wxBEGIN_EVENT_TABLE(CommandPalette, wxDialog)
    EVT_TEXT(ID_SEARCH_CTRL, CommandPalette::OnSearchTextChanged)
    EVT_LISTBOX_DCLICK(ID_COMMAND_LIST, CommandPalette::OnListItemActivated)
    EVT_LISTBOX(ID_COMMAND_LIST, CommandPalette::OnListItemSelected)
wxEND_EVENT_TABLE()

CommandPalette::CommandPalette(wxWindow* parent, CommandContext& context)
    : wxDialog(parent, wxID_ANY, "", wxDefaultPosition, wxSize(600, 400),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
    , m_context(context)
    , m_selectedCommand(nullptr)
{
    // Remove title bar text for cleaner look
    SetTitle("");
    
    CreateUI();
    UpdateCommandList();
    
    // Center on parent
    CentreOnParent();
    
    // Focus the search control
    m_searchCtrl->SetFocus();
}

void CommandPalette::CreateUI()
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Search control at the top
    m_searchCtrl = new wxSearchCtrl(this, ID_SEARCH_CTRL, "", 
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_PROCESS_ENTER);
    m_searchCtrl->SetDescriptiveText("Type to search commands...");
    m_searchCtrl->ShowCancelButton(true);
    
    // Make search control larger
    wxFont searchFont = m_searchCtrl->GetFont();
    searchFont.SetPointSize(searchFont.GetPointSize() + 2);
    m_searchCtrl->SetFont(searchFont);
    
    mainSizer->Add(m_searchCtrl, 0, wxEXPAND | wxALL, 8);
    
    // Command list
    m_listBox = new wxListBox(this, ID_COMMAND_LIST, 
                               wxDefaultPosition, wxDefaultSize,
                               0, nullptr,
                               wxLB_SINGLE | wxLB_NEEDED_SB);
    
    // Use a nice monospace-ish font for the list
    wxFont listFont = m_listBox->GetFont();
    listFont.SetPointSize(listFont.GetPointSize() + 1);
    m_listBox->SetFont(listFont);
    
    mainSizer->Add(m_listBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    
    SetSizer(mainSizer);
    
    // Bind key events for navigation
    m_searchCtrl->Bind(wxEVT_KEY_DOWN, &CommandPalette::OnKeyDown, this);
    m_listBox->Bind(wxEVT_KEY_DOWN, &CommandPalette::OnKeyDown, this);
    
    // Handle Enter in search control
    m_searchCtrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        ExecuteSelected();
    });
}

void CommandPalette::UpdateCommandList()
{
    wxString query = m_searchCtrl->GetValue();
    m_filteredCommands = CommandRegistry::Instance().Search(query, m_context);
    
    m_listBox->Clear();
    
    for (const auto& cmd : m_filteredCommands) {
        wxString displayText = cmd->GetDisplayString();
        
        // Add shortcut hint if available
        if (!cmd->GetShortcut().IsEmpty()) {
            displayText += "  [" + cmd->GetShortcut() + "]";
        }
        
        m_listBox->Append(displayText);
    }
    
    // Select first item if available
    if (m_listBox->GetCount() > 0) {
        m_listBox->SetSelection(0);
    }
}

void CommandPalette::SelectCommand(int index)
{
    if (index >= 0 && index < static_cast<int>(m_filteredCommands.size())) {
        m_listBox->SetSelection(index);
    }
}

void CommandPalette::ExecuteSelected()
{
    int selection = m_listBox->GetSelection();
    if (selection != wxNOT_FOUND && selection < static_cast<int>(m_filteredCommands.size())) {
        m_selectedCommand = m_filteredCommands[selection];
        EndModal(wxID_OK);
    }
}

void CommandPalette::OnSearchTextChanged(wxCommandEvent& event)
{
    UpdateCommandList();
    event.Skip();
}

void CommandPalette::OnListItemActivated(wxCommandEvent& event)
{
    ExecuteSelected();
}

void CommandPalette::OnListItemSelected(wxCommandEvent& event)
{
    // Just update selection, don't execute
    event.Skip();
}

void CommandPalette::OnKeyDown(wxKeyEvent& event)
{
    int keyCode = event.GetKeyCode();
    int selection = m_listBox->GetSelection();
    int count = m_listBox->GetCount();
    
    switch (keyCode) {
        case WXK_ESCAPE:
            EndModal(wxID_CANCEL);
            break;
            
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            ExecuteSelected();
            break;
            
        case WXK_UP:
            if (selection > 0) {
                m_listBox->SetSelection(selection - 1);
            } else if (count > 0) {
                m_listBox->SetSelection(count - 1);  // Wrap to bottom
            }
            break;
            
        case WXK_DOWN:
            if (selection < count - 1) {
                m_listBox->SetSelection(selection + 1);
            } else if (count > 0) {
                m_listBox->SetSelection(0);  // Wrap to top
            }
            break;
            
        case WXK_PAGEUP:
            if (count > 0) {
                int newSel = std::max(0, selection - 10);
                m_listBox->SetSelection(newSel);
            }
            break;
            
        case WXK_PAGEDOWN:
            if (count > 0) {
                int newSel = std::min(count - 1, selection + 10);
                m_listBox->SetSelection(newSel);
            }
            break;
            
        case WXK_HOME:
            if (count > 0 && event.ControlDown()) {
                m_listBox->SetSelection(0);
            } else {
                event.Skip();
            }
            break;
            
        case WXK_END:
            if (count > 0 && event.ControlDown()) {
                m_listBox->SetSelection(count - 1);
            } else {
                event.Skip();
            }
            break;
            
        default:
            event.Skip();
            break;
    }
}
