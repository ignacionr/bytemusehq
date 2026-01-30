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
#include "../mcp/mcp.h"
#include "../mcp/mcp_code_index.h"
#include "builtin_widgets.h"
#include "widget_bar.h"
#include "widget_activity_bar.h"
#include "symbols_widget.h"

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
    SetupActivityBar();     // Set up activity bar after widgets are registered
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
    
    // Horizontal splitter (left panel | right area) - create first as parent for children
    m_hSplitter = new wxSplitterWindow(m_mainPanel, wxID_ANY, 
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    // Left panel contains: Activity Bar | Tree/WidgetBar area
    // Parent must be m_hSplitter for SplitVertically to work
    m_leftPanel = new wxPanel(m_hSplitter);
    wxBoxSizer* leftPanelSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Activity bar (far left column with category buttons)
    m_activityBar = new WidgetActivityBar(m_leftPanel);
    m_activityBar->OnCategorySelected = [this](const wxString& categoryId) {
        OnCategorySelected(categoryId);
    };
    leftPanelSizer->Add(m_activityBar, 0, wxEXPAND);
    
    // Left splitter for tree/widget bar
    m_leftSplitter = new wxSplitterWindow(m_leftPanel, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    
    // Left content panel: Tree control
    m_leftContentPanel = new wxPanel(m_leftSplitter);
    wxBoxSizer* leftContentSizer = new wxBoxSizer(wxVERTICAL);
    
    m_treeCtrl = new wxTreeCtrl(m_leftContentPanel, wxID_ANY);
    leftContentSizer->Add(m_treeCtrl, 1, wxEXPAND | wxALL, 0);
    m_leftContentPanel->SetSizer(leftContentSizer);
    
    // Widget bar for sidebar widgets (Timer, Jira, etc.)
    m_widgetBar = new WidgetBar(m_leftSplitter, m_widgetContext);
    
    // Initially show only tree (widget bar added later if widgets are visible)
    m_leftSplitter->Initialize(m_leftContentPanel);
    m_leftSplitter->SetMinimumPaneSize(100);
    
    leftPanelSizer->Add(m_leftSplitter, 1, wxEXPAND);
    m_leftPanel->SetSizer(leftPanelSizer);
    
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
    m_hSplitter->SplitVertically(m_leftPanel, m_rightSplitter);
    m_hSplitter->SetSashPosition(300);  // Slightly wider to accommodate activity bar
    m_hSplitter->SetMinimumPaneSize(150);
    
    mainSizer->Add(m_hSplitter, 1, wxEXPAND);
    m_mainPanel->SetSizer(mainSizer);
    
    // Populate tree with current directory (or remote path if SSH enabled)
    m_sshConfig = FrameSshConfig::LoadFromConfig();
    if (m_sshConfig.isValid()) {
        // SSH enabled - use remote path and expand ~ to actual home directory
        wxString remotePath = Config::Instance().GetString("ssh.remotePath", "~");
        std::string expandedPath = m_sshConfig.expandRemotePath(remotePath.ToStdString());
        wxLogDebug("MainFrame: expanded remotePath '%s' -> '%s'", remotePath, expandedPath.c_str());
        OpenFolder(wxString(expandedPath), true);
    } else {
        // Local directory
        wxString currentDir = wxGetCwd();
        wxTreeItemId rootId = m_treeCtrl->AddRoot(currentDir);
        m_treeCtrl->SetItemData(rootId, new PathData(currentDir, false));
        PopulateTree(currentDir, rootId);
        m_treeCtrl->Expand(rootId);
    }
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
    
    // Left panel (sidebar container)
    if (m_leftPanel) {
        m_leftPanel->SetBackgroundColour(ui.sidebarBackground);
    }
    
    // Left content panel (tree area)
    if (m_leftContentPanel) {
        m_leftContentPanel->SetBackgroundColour(ui.sidebarBackground);
    }
    
    // Activity bar
    if (m_activityBar) {
        m_activityBar->ApplyTheme(theme);
    }
    
    // Widget bar
    if (m_widgetBar) {
        m_widgetBar->ApplyTheme(theme);
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
    wxLogMessage("MainFrame::OnTreeItemActivated: path='%s', isRemote=%d", path, data->IsRemote());
    
    if (data->IsRemote()) {
        // Remote file - check if it's a directory via SSH
        if (m_sshConfig.isValid()) {
            std::string sshPrefix = m_sshConfig.buildSshPrefix();
            std::string testCmd = sshPrefix + " \"test -d \\\"" + path.ToStdString() + "\\\"\" 2>&1";
            wxLogMessage("MainFrame::OnTreeItemActivated: testCmd='%s'", testCmd.c_str());
            int result = system(testCmd.c_str());
            wxLogMessage("MainFrame::OnTreeItemActivated: test result=%d", result);
            
            if (result != 0) {
                // It's a file, open it remotely
                m_editor->OpenRemoteFile(path, m_sshConfig.buildSshPrefix());
            }
            // If it's a directory, do nothing (expand handles it)
        }
    } else {
        // Local file - only open files, not directories
        if (!wxDir::Exists(path) && wxFileExists(path)) {
            m_editor->OpenFile(path);
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
        PathData* childData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(firstChild));
        if (!childData) {
            // This is the dummy child, remove it
            m_treeCtrl->Delete(firstChild);
            
            // Now populate the actual children
            PathData* parentData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
            if (parentData) {
                if (parentData->IsRemote()) {
                    PopulateTreeRemote(parentData->GetPath(), itemId);
                } else {
                    PopulateTree(parentData->GetPath(), itemId);
                }
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
    // Note: editorComponent is set in SetupSidebarWidgets after SetupUI creates the editor
}

void MainFrame::SetupSidebarWidgets()
{
    // Set editor in widget context now that it's created
    m_widgetContext.Set<Editor>("editorComponent", m_editor);
    m_widgetContext.Set<wxString>("workspaceRoot", new wxString(wxGetCwd()));
    
    // Get all sidebar widgets from the registry and add them to the widget bar
    auto& registry = WidgetRegistry::Instance();
    auto sidebarWidgets = registry.GetWidgetsByLocation(WidgetLocation::Sidebar);
    
    BuiltinWidgets::SymbolsWidget* symbolsWidget = nullptr;
    
    for (auto& widget : sidebarWidgets) {
        auto info = widget->GetInfo();
        // Skip FileTree widget as it has dedicated handling
        if (info.id == "core.fileTree") continue;
        
        // Track the symbols widget for MCP connection
        if (info.id == "core.symbols") {
            symbolsWidget = dynamic_cast<BuiltinWidgets::SymbolsWidget*>(widget.get());
        }
        
        // Add widget to the widget bar
        m_widgetBar->AddWidget(widget);
    }
    
    // Connect the symbols widget to the MCP code index provider
    if (symbolsWidget) {
        ConnectCodeIndexToMCP(symbolsWidget);
    }
    
    // Build the initial layout and update visibility
    m_widgetBar->RebuildLayout();
    UpdateWidgetBarVisibility();
}

/**
 * Connect the SymbolsWidget's code index to the MCP CodeIndexProvider.
 * This allows the AI chat to query the code index.
 */
void MainFrame::ConnectCodeIndexToMCP(BuiltinWidgets::SymbolsWidget* symbolsWidget)
{
    auto codeIndexProvider = std::dynamic_pointer_cast<MCP::CodeIndexProvider>(
        MCP::Registry::Instance().getProvider("mcp.codeindex"));
    
    if (!codeIndexProvider) {
        // Provider may not be registered yet if GeminiChatWidget hasn't been created
        // Create and register it now
        codeIndexProvider = std::make_shared<MCP::CodeIndexProvider>();
        MCP::Registry::Instance().registerProvider(codeIndexProvider);
    }
    
    // Set up callbacks to the symbols widget
    codeIndexProvider->setSearchCallback([symbolsWidget](const std::string& query) {
        auto results = symbolsWidget->SearchSymbols(wxString(query));
        std::vector<std::pair<std::string, LspDocumentSymbol>> stdResults;
        for (const auto& [path, sym] : results) {
            stdResults.push_back({path.ToStdString(), sym});
        }
        return stdResults;
    });
    
    codeIndexProvider->setFileSymbolsCallback([symbolsWidget](const std::string& path) {
        return symbolsWidget->GetFileSymbols(wxString(path));
    });
    
    codeIndexProvider->setAllSymbolsCallback([symbolsWidget]() {
        const auto& wxSymbols = symbolsWidget->GetAllSymbols();
        std::vector<std::pair<std::string, LspDocumentSymbol>> stdResults;
        stdResults.reserve(wxSymbols.size());
        for (const auto& [path, sym] : wxSymbols) {
            stdResults.push_back({path.ToStdString(), sym});
        }
        return stdResults;
    });
    
    codeIndexProvider->setSymbolsByKindCallback([symbolsWidget](LspSymbolKind kind) {
        auto results = symbolsWidget->GetSymbolsByKind(kind);
        std::vector<std::pair<std::string, LspDocumentSymbol>> stdResults;
        for (const auto& [path, sym] : results) {
            stdResults.push_back({path.ToStdString(), sym});
        }
        return stdResults;
    });
    
    codeIndexProvider->setIndexStatusCallback([symbolsWidget]() {
        return std::make_tuple(
            symbolsWidget->IsIndexingComplete(),
            symbolsWidget->GetIndexedFileCount(),
            symbolsWidget->GetIndexedSymbolCount()
        );
    });
}

void MainFrame::SetupActivityBar()
{
    // Add Explorer category for the file tree (always first)
    m_activityBar->AddCategory(WidgetCategories::Explorer());
    
    // Get categories from the widget bar
    auto categories = m_widgetBar->GetCategories();
    
    // Add categories to the activity bar
    for (const auto& category : categories) {
        m_activityBar->AddCategory(category);
    }
    
    // Select Explorer category by default
    m_activityBar->SelectCategory("explorer");
    OnCategorySelected("explorer");
}

void MainFrame::OnCategorySelected(const wxString& categoryId)
{
    m_currentCategory = categoryId;
    
    // Handle Explorer category specially - show file tree only
    if (categoryId == "explorer") {
        // Hide widget bar, show only file tree
        if (m_leftSplitter->IsSplit()) {
            m_leftSplitter->Unsplit(m_widgetBar);
        }
        m_widgetBar->Hide();
        m_leftContentPanel->Show();
        m_leftSplitter->Initialize(m_leftContentPanel);
        return;
    }
    
    // For other categories: hide file tree, show widget bar
    m_leftContentPanel->Hide();
    m_widgetBar->SetActiveCategory(categoryId);
    UpdateWidgetBarVisibility();
}

void MainFrame::UpdateWidgetBarVisibility()
{
    bool hasVisibleWidgets = m_widgetBar->HasVisibleWidgets();
    
    if (hasVisibleWidgets) {
        // Show widget bar only (file tree is hidden for non-Explorer categories)
        m_widgetBar->Show();
        if (m_leftSplitter->IsSplit()) {
            m_leftSplitter->Unsplit();
        }
        m_leftSplitter->Initialize(m_widgetBar);
        
        // Force layout update to ensure widgets are properly sized
        m_widgetBar->Layout();
        m_leftSplitter->Layout();
        m_leftPanel->Layout();
        
        // Queue another layout after the window is shown
        CallAfter([this]() {
            if (m_widgetBar) {
                m_widgetBar->Layout();
                m_widgetBar->Refresh();
            }
        });
    } else {
        // No visible widgets - show empty state or file tree
        if (m_leftSplitter->IsSplit()) {
            m_leftSplitter->Unsplit(m_widgetBar);
        }
        m_widgetBar->Hide();
    }
}

void MainFrame::ShowSidebarWidget(const wxString& widgetId, bool show)
{
    m_widgetBar->ShowWidget(widgetId, show);
    UpdateWidgetBarVisibility();
}

void MainFrame::ToggleSidebarWidget(const wxString& widgetId)
{
    m_widgetBar->ToggleWidget(widgetId);
    UpdateWidgetBarVisibility();
}

bool MainFrame::IsSidebarWidgetVisible(const wxString& widgetId) const
{
    return m_widgetBar->IsWidgetVisible(widgetId);
}

void MainFrame::NotifyThemeChanged()
{
    // Notify widget bar about theme change
    if (m_widgetBar) {
        m_widgetBar->NotifyThemeChanged();
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

void MainFrame::OpenFolder(const wxString& path, bool isRemote)
{
    m_isRemoteTree = isRemote;
    
    if (isRemote) {
        // Load SSH config for remote browsing
        m_sshConfig = FrameSshConfig::LoadFromConfig();
        if (!m_sshConfig.isValid()) {
            wxMessageBox("SSH is not properly configured. Please check ssh.host in config.",
                        "SSH Error", wxOK | wxICON_ERROR, this);
            return;
        }
        
        // Update terminal to use SSH if available
        if (m_terminal) {
            // Don't change local working directory for remote folders
            // Terminal will need its own SSH handling
        }
    } else {
        // Local folder
        if (!wxDir::Exists(path)) return;
        
        // Change the current working directory
        wxSetWorkingDirectory(path);
        
        // Update terminal working directory
        if (m_terminal) {
            m_terminal->SetWorkingDirectory(path);
        }
    }
    
    // Clear the existing tree
    m_treeCtrl->DeleteAllItems();
    
    // Populate tree with the new directory
    wxString displayPath = path;
    if (isRemote) {
        displayPath = "[SSH] " + path;
    }
    wxTreeItemId rootId = m_treeCtrl->AddRoot(displayPath);
    m_treeCtrl->SetItemData(rootId, new PathData(path, isRemote));
    
    if (isRemote) {
        PopulateTreeRemote(path, rootId);
    } else {
        PopulateTree(path, rootId);
    }
    m_treeCtrl->Expand(rootId);
    
    // Update the window title to reflect the change
    UpdateTitle();
}

void MainFrame::PopulateTreeRemote(const wxString& path, wxTreeItemId parentItem)
{
    if (!m_sshConfig.isValid()) return;
    
    std::string sshPrefix = m_sshConfig.buildSshPrefix();
    std::string cmd = sshPrefix + " \"ls -la '" + path.ToStdString() + "' 2>/dev/null\" 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;
    
    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);
    
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Skip total line and empty lines
        if (line.empty() || line.find("total") == 0) continue;
        
        // Parse ls -la output
        std::istringstream lineStream(line);
        std::string permissions, links, owner, group, size, month, day, timeStr, name;
        lineStream >> permissions >> links >> owner >> group >> size >> month >> day >> timeStr;
        std::getline(lineStream, name);
        
        // Trim leading whitespace from name
        size_t start = name.find_first_not_of(" \t");
        if (start != std::string::npos) {
            name = name.substr(start);
        }
        
        // Skip . and .. and hidden files
        if (name.empty() || name == "." || name == ".." || name[0] == '.') continue;
        
        wxString fullPath = path;
        if (!fullPath.EndsWith("/")) fullPath += "/";
        fullPath += wxString(name);
        
        bool isDirectory = !permissions.empty() && permissions[0] == 'd';
        
        if (isDirectory) {
            wxTreeItemId itemId = m_treeCtrl->AppendItem(
                parentItem, wxString(name), -1, -1, new PathData(fullPath, true));
            m_treeCtrl->AppendItem(itemId, ""); // Dummy for expand arrow
        } else {
            m_treeCtrl->AppendItem(
                parentItem, wxString(name), -1, -1, new PathData(fullPath, true));
        }
    }
    m_treeCtrl->SortChildren(parentItem);
}
