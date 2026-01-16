#include "frame.h"
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/splitter.h>
#include "../commands/command_registry.h"
#include "../commands/command_palette.h"
#include "../commands/builtin_commands.h"

enum {
    ID_COMMAND_PALETTE = wxID_HIGHEST + 1
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, MainFrame::OnTreeItemActivated)
    EVT_TREE_ITEM_COLLAPSING(wxID_ANY, MainFrame::OnTreeItemCollapsing)
    EVT_TREE_ITEM_EXPANDING(wxID_ANY, MainFrame::OnTreeItemExpanding)
    EVT_MENU(ID_COMMAND_PALETTE, MainFrame::OnCommandPalette)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "ByteMuseHQ", wxDefaultPosition, wxSize(1000, 600))
{
    RegisterCommands();
    SetupUI();
    SetupAccelerators();
}

void MainFrame::SetupUI()
{
    // Create main panel and sizer
    wxPanel* mainPanel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Create splitter window
    wxSplitterWindow* splitter = new wxSplitterWindow(mainPanel);
    
    // Left panel: Tree control
    wxPanel* leftPanel = new wxPanel(splitter);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    
    m_treeCtrl = new wxTreeCtrl(leftPanel, wxID_ANY);
    leftSizer->Add(m_treeCtrl, 1, wxEXPAND | wxALL, 0);
    leftPanel->SetSizer(leftSizer);
    
    // Right panel: Editor
    wxPanel* rightPanel = new wxPanel(splitter);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    
    m_editor = new wxStyledTextCtrl(rightPanel, wxID_ANY);
    
    // Configure editor
    m_editor->SetLexer(wxSTC_LEX_CPP);
    m_editor->StyleClearAll();
    m_editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    m_editor->SetMarginWidth(0, 30);
    m_editor->SetTabWidth(4);
    m_editor->SetUseTabs(false);
    
    rightSizer->Add(m_editor, 1, wxEXPAND | wxALL, 0);
    rightPanel->SetSizer(rightSizer);
    
    // Split the splitter
    splitter->SplitVertically(leftPanel, rightPanel);
    splitter->SetSashPosition(250);
    splitter->SetMinimumPaneSize(100);
    
    mainSizer->Add(splitter, 1, wxEXPAND);
    mainPanel->SetSizer(mainSizer);
    
    // Populate tree with current directory
    wxString currentDir = wxGetCwd();
    wxTreeItemId rootId = m_treeCtrl->AddRoot(currentDir);
    PopulateTree(currentDir, rootId);
    m_treeCtrl->Expand(rootId);
}

void MainFrame::PopulateTree(const wxString& path, wxTreeItemId parentItem)
{
    wxDir dir(path);
    if (!dir.IsOpened())
        return;
    
    wxString filename;
    bool cont = dir.GetFirst(&filename);
    
    while (cont) {
        // Skip hidden files/directories (starting with .)
        if (!filename.StartsWith(".")) {
            wxString fullPath = wxFileName(path, filename).GetFullPath();
            
            if (wxDir::Exists(fullPath)) {
                // It's a directory
                wxTreeItemId itemId = m_treeCtrl->AppendItem(
                    parentItem, filename, -1, -1,
                    new PathData(fullPath)
                );
                // Add a dummy child to show expand arrow
                m_treeCtrl->AppendItem(itemId, "");
            } else {
                // It's a file
                m_treeCtrl->AppendItem(
                    parentItem, filename, -1, -1,
                    new PathData(fullPath)
                );
            }
        }
        cont = dir.GetNext(&filename);
    }
    
    // Sort items
    m_treeCtrl->SortChildren(parentItem);
}

void MainFrame::OnTreeItemActivated(wxTreeEvent& event)
{
    wxTreeItemId itemId = event.GetItem();
    PathData* data = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
    
    if (!data)
        return;
    
    wxString path = data->GetPath();
    
    // Only open files, not directories
    if (!wxDir::Exists(path) && wxFileExists(path)) {
        wxFile file(path);
        if (file.IsOpened()) {
            wxString content;
            file.ReadAll(&content);
            m_editor->SetText(content);
            m_editor->EmptyUndoBuffer();
        }
    }
}

void MainFrame::OnTreeItemCollapsing(wxTreeEvent& event)
{
    // Allow collapse
}

void MainFrame::OnTreeItemExpanding(wxTreeEvent& event)
{
    wxTreeItemId itemId = event.GetItem();
    
    // Remove dummy child and populate on first expansion
    wxTreeItemIdValue cookie;
    wxTreeItemId firstChild = m_treeCtrl->GetFirstChild(itemId, cookie);
    
    if (firstChild.IsOk()) {
        PathData* data = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(firstChild));
        if (!data) {
            // This is the dummy child, remove it
            m_treeCtrl->Delete(firstChild);
            
            // Now populate the actual children
            PathData* parentData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
            if (parentData) {
                PopulateTree(parentData->GetPath(), itemId);
            }
        }
    }
}

void MainFrame::RegisterCommands()
{
    // Register all built-in commands
    BuiltinCommands::RegisterAll();
}

void MainFrame::SetupAccelerators()
{
    // Set up keyboard shortcut for command palette (Ctrl+Shift+P)
    wxAcceleratorEntry entries[1];
    entries[0].Set(wxACCEL_CTRL | wxACCEL_SHIFT, 'P', ID_COMMAND_PALETTE);
    wxAcceleratorTable accel(1, entries);
    SetAcceleratorTable(accel);
}

CommandContext MainFrame::CreateCommandContext()
{
    CommandContext ctx;
    ctx.Set<wxWindow>("window", this);
    ctx.Set<wxStyledTextCtrl>("editor", m_editor);
    ctx.Set<wxString>("currentFile", &m_currentFilePath);
    return ctx;
}

void MainFrame::ShowCommandPalette()
{
    CommandContext ctx = CreateCommandContext();
    CommandPalette palette(this, ctx);
    
    if (palette.ShowModal() == wxID_OK) {
        auto cmd = palette.GetSelectedCommand();
        if (cmd) {
            // Don't recursively open command palette
            if (cmd->GetId() != "app.commandPalette") {
                cmd->Execute(ctx);
            }
        }
    }
}

void MainFrame::OnCommandPalette(wxCommandEvent& event)
{
    ShowCommandPalette();
}
