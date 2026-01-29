#include "frame.h"
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/splitter.h>
#include <wx/filedlg.h>
#include "../commands/command_registry.h"
#include "../commands/command_palette.h"
#include "../commands/builtin_commands.h"
#include "../theme/theme.h"
#include "builtin_widgets.h"

enum {
    ID_COMMAND_PALETTE = wxID_HIGHEST + 1,
    ID_NEW_FILE,
    ID_OPEN_FILE,
    ID_SAVE,
    ID_SAVE_AS,
    ID_TOGGLE_TERMINAL
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_TREE_ITEM_ACTIVATED(wxID_ANY, MainFrame::OnTreeItemActivated)
    EVT_TREE_ITEM_COLLAPSING(wxID_ANY, MainFrame::OnTreeItemCollapsing)
    EVT_TREE_ITEM_EXPANDING(wxID_ANY, MainFrame::OnTreeItemExpanding)
    EVT_MENU(ID_COMMAND_PALETTE, MainFrame::OnCommandPalette)
    EVT_MENU(ID_NEW_FILE, MainFrame::OnNewFile)
    EVT_MENU(ID_OPEN_FILE, MainFrame::OnOpenFile)
    EVT_MENU(ID_SAVE, MainFrame::OnSave)
    EVT_MENU(ID_SAVE_AS, MainFrame::OnSaveAs)
    EVT_MENU(ID_TOGGLE_TERMINAL, MainFrame::OnToggleTerminal)
    EVT_CLOSE(MainFrame::OnClose)
wxEND_EVENT_TABLE()

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "ByteMuseHQ", wxDefaultPosition, wxSize(1000, 600))
    , m_themeListenerId(0)
{
    RegisterCommands();
    RegisterWidgets();
    SetupUI();
    SetupSidebarWidgets();  // Initialize sidebar widgets after UI is set up
    SetupMenuBar();
    SetupAccelerators();
    ApplyCurrentTheme();
    NotifyThemeChanged();   // Apply theme to widgets
    UpdateTitle();
    
    // Listen for theme changes
    m_themeListenerId = ThemeManager::Instance().AddChangeListener(
        [this](const ThemePtr& theme) {
            ApplyTheme(theme);
            NotifyThemeChanged();
        });
}

MainFrame::~MainFrame()
{
    if (m_themeListenerId > 0) {
        ThemeManager::Instance().RemoveChangeListener(m_themeListenerId);
    }
}

void MainFrame::SetupUI()
{
    // Create main panel and sizer
    m_mainPanel = new wxPanel(this);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Create horizontal splitter window (left sidebar | right area)
    m_hSplitter = new wxSplitterWindow(m_mainPanel, wxID_ANY, 
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    // Left area: Vertical splitter for tree/sidebar widgets
    m_leftSplitter = new wxSplitterWindow(m_hSplitter, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    // Left panel: Tree control
    m_leftPanel = new wxPanel(m_leftSplitter);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    
    m_treeCtrl = new wxTreeCtrl(m_leftPanel, wxID_ANY);
    leftSizer->Add(m_treeCtrl, 1, wxEXPAND | wxALL, 0);
    m_leftPanel->SetSizer(leftSizer);
    
    // Sidebar widget container (for widgets like Timer)
    m_sidebarWidgetContainer = new wxPanel(m_leftSplitter);
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    m_sidebarWidgetContainer->SetSizer(sidebarSizer);
    
    // Initially show only tree (sidebar widgets added later)
    m_leftSplitter->Initialize(m_leftPanel);
    m_leftSplitter->SetMinimumPaneSize(100);
    
    // Right area: Vertical splitter for editor/terminal
    m_rightSplitter = new wxSplitterWindow(m_hSplitter, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    // Editor component
    m_editor = new Editor(m_rightSplitter);
    
    // Set up callbacks for dirty state and file changes
    m_editor->SetDirtyStateCallback([this](bool isDirty) {
        UpdateTitle();
    });
    
    m_editor->SetFileChangeCallback([this](const wxString& filePath) {
        UpdateTitle();
    });
    
    // Terminal component
    m_terminal = new Terminal(m_rightSplitter);
    
    // Initially show only editor (terminal hidden)
    m_rightSplitter->Initialize(m_editor);
    m_rightSplitter->SetMinimumPaneSize(100);
    
    // Split horizontal (left | right)
    m_hSplitter->SplitVertically(m_leftSplitter, m_rightSplitter);
    m_hSplitter->SetSashPosition(250);
    m_hSplitter->SetMinimumPaneSize(100);
    
    mainSizer->Add(m_hSplitter, 1, wxEXPAND);
    m_mainPanel->SetSizer(mainSizer);
    
    // Populate tree with current directory
    wxString currentDir = wxGetCwd();
    wxTreeItemId rootId = m_treeCtrl->AddRoot(currentDir);
    PopulateTree(currentDir, rootId);
    m_treeCtrl->Expand(rootId);
}

void MainFrame::ApplyCurrentTheme()
{
    auto theme = ThemeManager::Instance().GetCurrentTheme();
    if (theme) {
        ApplyTheme(theme);
    }
}

void MainFrame::ApplyTheme(const ThemePtr& theme)
{
    if (!theme) return;
    
    const auto& ui = theme->ui;
    
    // Main frame background
    SetBackgroundColour(ui.windowBackground);
    
    // Main panel
    if (m_mainPanel) {
        m_mainPanel->SetBackgroundColour(ui.windowBackground);
    }
    
    // Left panel (sidebar)
    if (m_leftPanel) {
        m_leftPanel->SetBackgroundColour(ui.sidebarBackground);
    }
    
    // Sidebar widget container
    if (m_sidebarWidgetContainer) {
        m_sidebarWidgetContainer->SetBackgroundColour(ui.sidebarBackground);
    }
    
    // Left splitter
    if (m_leftSplitter) {
        m_leftSplitter->SetBackgroundColour(ui.border);
    }
    
    // Tree control (sidebar)
    if (m_treeCtrl) {
        m_treeCtrl->SetBackgroundColour(ui.sidebarBackground);
        m_treeCtrl->SetForegroundColour(ui.sidebarForeground);
    }
    
    // Splitters
    if (m_hSplitter) {
        m_hSplitter->SetBackgroundColour(ui.border);
    }
    if (m_rightSplitter) {
        m_rightSplitter->SetBackgroundColour(ui.border);
    }
    
    // Sidebar splitters (for resizable widgets)
    for (auto* splitter : m_sidebarSplitters) {
        if (splitter) {
            splitter->SetBackgroundColour(ui.border);
        }
    }
    
    // Refresh all
    Refresh();
    Update();
}

void MainFrame::SetupMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar();
    
    // File menu
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_NEW_FILE, "New\tCtrl+N", "Create a new file");
    fileMenu->Append(ID_OPEN_FILE, "Open...\tCtrl+O", "Open an existing file");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_SAVE, "Save\tCtrl+S", "Save the current file");
    fileMenu->Append(ID_SAVE_AS, "Save As...\tCtrl+Shift+S", "Save with a new name");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "Exit\tAlt+F4", "Exit the application");
    
    // View menu
    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(ID_COMMAND_PALETTE, "Command Palette\tCtrl+Shift+P", "Open command palette");
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_TOGGLE_TERMINAL, "Toggle Terminal\tCtrl+`", "Show or hide terminal");
    
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(viewMenu, "&View");
    
    SetMenuBar(menuBar);
}

void MainFrame::UpdateTitle()
{
    wxString title = "ByteMuseHQ";
    if (m_editor) {
        wxString fileTitle = m_editor->GetTitle();
        if (!fileTitle.IsEmpty() && fileTitle != "Untitled") {
            title = fileTitle + " - " + title;
        } else if (m_editor->IsModified()) {
            title = "• Untitled - " + title;
        }
    }
    SetTitle(title);
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
        m_editor->OpenFile(path);
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

void MainFrame::RegisterWidgets()
{
    // Register all built-in widgets
    BuiltinWidgets::RegisterAll();
    
    // Set up widget context with frame references
    m_widgetContext.Set<wxWindow>("window", this);
    m_widgetContext.Set<MainFrame>("mainFrame", this);
}

void MainFrame::SetupSidebarWidgets()
{
    // Get all sidebar widgets from the registry and create their windows
    auto& registry = WidgetRegistry::Instance();
    auto sidebarWidgets = registry.GetWidgetsByLocation(WidgetLocation::Sidebar);
    
    for (auto& widget : sidebarWidgets) {
        auto info = widget->GetInfo();
        // Skip FileTree widget as it has dedicated handling
        if (info.id == "core.fileTree") continue;
        
        // Create widget window (initially hidden, parented to container)
        wxWindow* widgetWindow = widget->CreateWindow(m_sidebarWidgetContainer, m_widgetContext);
        if (widgetWindow) {
            m_sidebarWidgetWindows[info.id] = widgetWindow;
            widget->RegisterCommands(m_widgetContext);
            widget->OnThemeChanged(widgetWindow, m_widgetContext);
            widgetWindow->Hide();  // Start hidden, RebuildSidebarLayout will show visible ones
            
            // Track visibility based on showByDefault
            if (info.showByDefault) {
                m_visibleSidebarWidgets.insert(info.id);
            }
        }
    }
    
    // Build the initial layout
    RebuildSidebarLayout();
}

void MainFrame::RebuildSidebarLayout()
{
    wxBoxSizer* containerSizer = dynamic_cast<wxBoxSizer*>(m_sidebarWidgetContainer->GetSizer());
    if (!containerSizer) return;
    
    // Detach all items from sizer (don't destroy windows)
    containerSizer->Clear(false);
    
    // IMPORTANT: Reparent all widgets back to container BEFORE destroying splitters
    // This prevents widgets from being destroyed along with their parent splitters
    for (auto& [id, window] : m_sidebarWidgetWindows) {
        if (window && window->GetParent() != m_sidebarWidgetContainer) {
            window->Hide();
            window->Reparent(m_sidebarWidgetContainer);
        }
    }
    
    // Now safe to destroy old splitters (they have no children)
    for (auto* splitter : m_sidebarSplitters) {
        splitter->Destroy();
    }
    m_sidebarSplitters.clear();
    
    // Collect visible widgets in order
    auto& registry = WidgetRegistry::Instance();
    auto sidebarWidgets = registry.GetWidgetsByLocation(WidgetLocation::Sidebar);
    
    std::vector<std::pair<wxString, wxWindow*>> visibleWidgets;
    for (auto& widget : sidebarWidgets) {
        auto info = widget->GetInfo();
        if (info.id == "core.fileTree") continue;
        
        auto it = m_sidebarWidgetWindows.find(info.id);
        if (it != m_sidebarWidgetWindows.end() && m_visibleSidebarWidgets.count(info.id)) {
            visibleWidgets.push_back({info.id, it->second});
        }
    }
    
    // Hide all widget windows first
    for (auto& [id, window] : m_sidebarWidgetWindows) {
        if (window) window->Hide();
    }
    
    if (visibleWidgets.empty()) {
        // No visible widgets - hide the container
        if (m_leftSplitter->IsSplit()) {
            m_leftSplitter->Unsplit(m_sidebarWidgetContainer);
        }
        m_sidebarWidgetContainer->Hide();
        return;
    }
    
    // Build layout for visible widgets
    if (visibleWidgets.size() == 1) {
        // Single widget - add directly to container (already parented to container)
        auto& [id, window] = visibleWidgets[0];
        containerSizer->Add(window, 1, wxEXPAND);
        window->Show();
    } else {
        // Multiple widgets - use nested splitters
        size_t numWidgets = visibleWidgets.size();
        size_t numSplitters = numWidgets - 1;
        
        // Create splitters chain
        for (size_t i = 0; i < numSplitters; ++i) {
            wxWindow* parent = (i == 0) ? 
                static_cast<wxWindow*>(m_sidebarWidgetContainer) : 
                static_cast<wxWindow*>(m_sidebarSplitters[i - 1]);
            
            wxSplitterWindow* splitter = new wxSplitterWindow(
                parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxSP_LIVE_UPDATE | wxSP_3DSASH);
            splitter->SetMinimumPaneSize(60);
            m_sidebarSplitters.push_back(splitter);
        }
        
        // Reparent visible widgets to their splitters
        for (size_t i = 0; i < numWidgets; ++i) {
            auto& [id, window] = visibleWidgets[i];
            wxWindow* widgetParent;
            if (i < numSplitters) {
                widgetParent = m_sidebarSplitters[i];
            } else {
                widgetParent = m_sidebarSplitters[numSplitters - 1];
            }
            window->Reparent(widgetParent);
            window->Show();
        }
        
        // Perform splits from innermost to outermost
        for (int i = (int)numSplitters - 1; i >= 0; --i) {
            wxSplitterWindow* splitter = m_sidebarSplitters[i];
            wxWindow* topPane = visibleWidgets[i].second;
            wxWindow* bottomPane;
            
            if (i == (int)numSplitters - 1) {
                bottomPane = visibleWidgets[i + 1].second;
            } else {
                bottomPane = m_sidebarSplitters[i + 1];
            }
            
            if (topPane && bottomPane) {
                splitter->SplitHorizontally(topPane, bottomPane);
                double gravity = 1.0 / (numWidgets - i);
                splitter->SetSashGravity(gravity);
            }
        }
        
        // Add root splitter to container
        containerSizer->Add(m_sidebarSplitters[0], 1, wxEXPAND);
        
        // Apply theme to new splitters
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (theme) {
            for (auto* splitter : m_sidebarSplitters) {
                splitter->SetBackgroundColour(theme->ui.border);
            }
        }
    }
    
    // Show container and split if needed
    m_sidebarWidgetContainer->Show();
    if (!m_leftSplitter->IsSplit()) {
        m_leftSplitter->SplitHorizontally(m_leftPanel, m_sidebarWidgetContainer);
        m_leftSplitter->SetSashGravity(0.5);
        int height = m_leftSplitter->GetSize().GetHeight();
        m_leftSplitter->SetSashPosition(height / 2);
    }
    
    m_sidebarWidgetContainer->Layout();
    m_sidebarWidgetContainer->Refresh();
}

void MainFrame::ShowSidebarWidget(const wxString& widgetId, bool show)
{
    // Check if widget exists
    if (m_sidebarWidgetWindows.find(widgetId) == m_sidebarWidgetWindows.end()) return;
    
    bool currentlyVisible = m_visibleSidebarWidgets.count(widgetId) > 0;
    if (currentlyVisible == show) return;  // No change needed
    
    // Update visibility tracking
    if (show) {
        m_visibleSidebarWidgets.insert(widgetId);
    } else {
        m_visibleSidebarWidgets.erase(widgetId);
    }
    
    // Rebuild the layout with the new visibility state
    RebuildSidebarLayout();
}

void MainFrame::ToggleSidebarWidget(const wxString& widgetId)
{
    ShowSidebarWidget(widgetId, !IsSidebarWidgetVisible(widgetId));
}

bool MainFrame::IsSidebarWidgetVisible(const wxString& widgetId) const
{
    return m_visibleSidebarWidgets.count(widgetId) > 0;
}

void MainFrame::NotifyThemeChanged()
{
    // Notify all widgets about theme change
    auto& registry = WidgetRegistry::Instance();
    
    for (const auto& widget : registry.GetAllWidgets()) {
        auto it = m_sidebarWidgetWindows.find(widget->GetInfo().id);
        if (it != m_sidebarWidgetWindows.end() && it->second) {
            widget->OnThemeChanged(it->second, m_widgetContext);
        }
    }
}

void MainFrame::SetupAccelerators()
{
    // Set up keyboard shortcuts
    wxAcceleratorEntry entries[2];
    entries[0].Set(wxACCEL_CTRL | wxACCEL_SHIFT, 'P', ID_COMMAND_PALETTE);
    entries[1].Set(wxACCEL_CTRL, '`', ID_TOGGLE_TERMINAL);
    wxAcceleratorTable accel(2, entries);
    SetAcceleratorTable(accel);
}

CommandContext MainFrame::CreateCommandContext()
{
    CommandContext ctx;
    ctx.Set<wxWindow>("window", this);
    ctx.Set<MainFrame>("mainFrame", this);
    ctx.Set<wxStyledTextCtrl>("editor", m_editor->GetTextCtrl());
    ctx.Set<Editor>("editorComponent", m_editor);
    ctx.Set<Terminal>("terminal", m_terminal);
    wxString currentFile = m_editor->GetFilePath();
    ctx.Set<wxString>("currentFile", &currentFile);
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

void MainFrame::OnNewFile(wxCommandEvent& event)
{
    m_editor->NewFile();
}

void MainFrame::OnOpenFile(wxCommandEvent& event)
{
    wxFileDialog dlg(this, "Open File", "", "",
                     "All files (*.*)|*.*|"
                     "C++ files (*.cpp;*.h;*.hpp)|*.cpp;*.h;*.hpp|"
                     "Text files (*.txt)|*.txt",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    
    if (dlg.ShowModal() == wxID_OK) {
        m_editor->OpenFile(dlg.GetPath());
    }
}

void MainFrame::OnSave(wxCommandEvent& event)
{
    m_editor->Save();
}

void MainFrame::OnSaveAs(wxCommandEvent& event)
{
    m_editor->SaveAs();
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    if (event.CanVeto() && m_editor->IsModified()) {
        if (!m_editor->PromptSaveIfModified()) {
            event.Veto();
            return;
        }
    }
    event.Skip();
}

void MainFrame::ShowTerminal(bool show)
{
    if (show) {
        if (!m_rightSplitter->IsSplit()) {
            // Show terminal by splitting
            m_terminal->Show();
            m_rightSplitter->SplitHorizontally(m_editor, m_terminal);
            // Position sash at 70% height (editor gets more space)
            int height = m_rightSplitter->GetSize().GetHeight();
            m_rightSplitter->SetSashPosition(height * 7 / 10);
        }
        // Focus the terminal input
        m_terminal->SetFocus();
    }
    else {
        if (m_rightSplitter->IsSplit()) {
            // Hide terminal by unsplitting
            m_rightSplitter->Unsplit(m_terminal);
            m_terminal->Hide();
        }
    }
}

void MainFrame::ToggleTerminal()
{
    ShowTerminal(!IsTerminalVisible());
}

bool MainFrame::IsTerminalVisible() const
{
    return m_rightSplitter->IsSplit();
}

void MainFrame::OnToggleTerminal(wxCommandEvent& event)
{
    ToggleTerminal();
}

void MainFrame::OpenFolder(const wxString& path)
{
    if (!wxDir::Exists(path)) return;
    
    // Change the current working directory
    wxSetWorkingDirectory(path);
    
    // Update terminal working directory
    if (m_terminal) {
        m_terminal->SetWorkingDirectory(path);
    }
    
    // Clear the existing tree
    m_treeCtrl->DeleteAllItems();
    
    // Populate tree with the new directory
    wxTreeItemId rootId = m_treeCtrl->AddRoot(path);
    PopulateTree(path, rootId);
    m_treeCtrl->Expand(rootId);
    
    // Update the window title to reflect the change
    UpdateTitle();
}
