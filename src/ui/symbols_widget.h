#ifndef SYMBOLS_WIDGET_H
#define SYMBOLS_WIDGET_H

#include "widget.h"
#include "editor.h"
#include "../lsp/lsp_client.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../fs/fs.h"
#include <wx/treectrl.h>
#include <wx/textctrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <memory>
#include <set>

namespace BuiltinWidgets {

/**
 * Tree item data to store symbol information.
 */
class SymbolData : public wxTreeItemData {
public:
    SymbolData(const wxString& filePath, const LspDocumentSymbol& symbol) 
        : m_filePath(filePath), m_symbol(symbol), m_isFile(false) {}
    
    SymbolData(const wxString& filePath) 
        : m_filePath(filePath), m_isFile(true) {}
    
    const wxString& GetFilePath() const { return m_filePath; }
    const LspDocumentSymbol& GetSymbol() const { return m_symbol; }
    bool IsFile() const { return m_isFile; }
    
private:
    wxString m_filePath;
    LspDocumentSymbol m_symbol;
    bool m_isFile;
};

/**
 * Workspace code index widget.
 * 
 * Indexes all source files in the workspace using clangd,
 * providing a searchable symbol database for:
 * - Go to symbol
 * - Find all symbols in workspace
 * - Expose to MCP for AI chat integration
 * 
 * Features:
 * - Recursive directory scanning
 * - Background indexing
 * - Search/filter symbols
 * - Click to navigate
 */
class SymbolsWidget : public Widget {
public:
    ~SymbolsWidget() {
        // Mark as destroyed to prevent callbacks from using invalid 'this'
        m_destroyed = true;
        
        // Stop and cleanup timer
        if (m_indexTimeoutTimer) {
            m_indexTimeoutTimer->Stop();
            delete m_indexTimeoutTimer;
            m_indexTimeoutTimer = nullptr;
        }
        
        // Clear log callback BEFORE destroying LspClient to prevent 
        // crashes during shutdown when wxTheApp may be null
        if (m_lspClient) {
            m_lspClient->setLogCallback(nullptr);
            m_lspClient->stop();
        }
    }
    
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.symbols";
        info.name = "Code Index";
        info.description = "Workspace code symbols index";
        info.location = WidgetLocation::Sidebar;
        info.category = WidgetCategories::Code();
        info.priority = 100;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // Header with title
        wxPanel* headerPanel = new wxPanel(m_panel);
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
        
        m_titleLabel = new wxStaticText(headerPanel, wxID_ANY, "CODE INDEX");
        headerSizer->Add(m_titleLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        
        m_refreshButton = new wxButton(headerPanel, wxID_ANY, wxString::FromUTF8("⟳"), 
            wxDefaultPosition, wxSize(24, 24), wxBORDER_NONE);
        m_refreshButton->SetToolTip("Re-index workspace");
        headerSizer->Add(m_refreshButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        
        headerPanel->SetSizer(headerSizer);
        mainSizer->Add(headerPanel, 0, wxEXPAND);
        
        // Search box - using wxTextCtrl for better color control
        m_searchCtrl = new wxTextCtrl(m_panel, wxID_ANY, "", 
            wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        m_searchCtrl->SetHint("Search symbols...");
        mainSizer->Add(m_searchCtrl, 0, wxEXPAND | wxALL, 4);
        
        // Status label
        m_statusLabel = new wxStaticText(m_panel, wxID_ANY, "Initializing...", 
            wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
        mainSizer->Add(m_statusLabel, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        
        // Tree control for symbols
        m_treeCtrl = new wxTreeCtrl(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT);
        m_treeCtrl->AddRoot("Workspace");
        mainSizer->Add(m_treeCtrl, 1, wxEXPAND);
        
        m_panel->SetSizer(mainSizer);
        
        // Store context
        m_context = &context;
        
        // Determine remote mode and workspace root
        auto& config = Config::Instance();
        m_isRemoteMode = config.GetBool("ssh.enabled", false);
        
        if (m_isRemoteMode) {
            // Use remote path as workspace root
            wxString remotePath = config.GetString("ssh.remotePath", "~");
            auto sshConfig = FS::SshConfig::LoadFromConfig();
            m_workspaceRoot = wxString(sshConfig.expandRemotePath(remotePath.ToStdString()));
            m_titleLabel->SetLabel("CODE INDEX [SSH]");
        } else {
            // Get local workspace root
            auto* rootPtr = context.Get<wxString>("workspaceRoot");
            m_workspaceRoot = rootPtr ? *rootPtr : wxGetCwd();
        }
        
        // Bind events
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &SymbolsWidget::OnItemActivated, this);
        m_refreshButton->Bind(wxEVT_BUTTON, &SymbolsWidget::OnRefreshClicked, this);
        m_searchCtrl->Bind(wxEVT_TEXT, &SymbolsWidget::OnSearchTextChanged, this);
        m_searchCtrl->Bind(wxEVT_TEXT_ENTER, &SymbolsWidget::OnSearch, this);
        
        // Initialize LSP and start indexing
        InitializeLspClient();
        
        // Apply theme
        OnThemeChanged(m_panel, context);
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme || !m_panel) return;
        
        m_panel->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetForegroundColour(theme->ui.sidebarForeground);
        
        m_titleLabel->SetForegroundColour(theme->ui.sidebarForeground);
        m_statusLabel->SetForegroundColour(theme->ui.sidebarForeground);
        
        // Set colors for search control
        m_searchCtrl->SetBackgroundColour(theme->palette.inputBackground);
        m_searchCtrl->SetForegroundColour(theme->palette.inputForeground);
        
        m_refreshButton->SetBackgroundColour(theme->ui.sidebarBackground);
        m_refreshButton->SetForegroundColour(theme->ui.sidebarForeground);
        
        m_panel->Refresh();
    }

    void OnShow(wxWindow* window, WidgetContext& context) override {
        // Trigger indexing if not already done
        if (m_allSymbols.empty() && m_lspClient && m_lspClient->isInitialized()) {
            StartIndexing();
        }
    }

    std::vector<wxString> GetCommands() const override {
        return {
            "symbols.reindex",
            "symbols.goToSymbol",
            "symbols.search"
        };
    }

    void RegisterCommands(WidgetContext& context) override {
        auto& registry = CommandRegistry::Instance();
        
        auto reindexCmd = std::make_shared<Command>(
            "symbols.reindex", "Reindex Workspace", "Code"
        );
        reindexCmd->SetDescription("Rebuild the workspace symbol index");
        reindexCmd->SetExecuteHandler([this](CommandContext& ctx) {
            StartIndexing();
        });
        registry.Register(reindexCmd);
        
        auto goToCmd = std::make_shared<Command>(
            "symbols.goToSymbol", "Go to Symbol in Workspace", "Go"
        );
        goToCmd->SetShortcut("Ctrl+T");
        goToCmd->SetDescription("Search and navigate to any symbol in the workspace");
        goToCmd->SetExecuteHandler([this](CommandContext& ctx) {
            if (m_searchCtrl) {
                m_searchCtrl->SetFocus();
                m_searchCtrl->SelectAll();
            }
        });
        registry.Register(goToCmd);
    }

    // ========================================================================
    // Public API for MCP integration
    // ========================================================================
    
    /**
     * Get all indexed symbols.
     * Returns a vector of all symbols across the workspace.
     */
    const std::vector<std::pair<std::string, LspDocumentSymbol>>& GetAllSymbols() const {
        return m_allSymbols;
    }
    
    /**
     * Search symbols by name (fuzzy match).
     */
    std::vector<std::pair<std::string, LspDocumentSymbol>> SearchSymbols(const std::string& query) const {
        std::vector<std::pair<std::string, LspDocumentSymbol>> results;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        for (const auto& [filePath, symbol] : m_allSymbols) {
            std::string lowerName = symbol.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find(lowerQuery) != std::string::npos) {
                results.push_back({filePath, symbol});
            }
        }
        
        // Sort by relevance (exact prefix match first, then contains)
        std::sort(results.begin(), results.end(), 
            [&lowerQuery](const auto& a, const auto& b) {
                std::string aLower = a.second.name;
                std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
                std::string bLower = b.second.name;
                std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
                bool aPrefix = aLower.find(lowerQuery) == 0;
                bool bPrefix = bLower.find(lowerQuery) == 0;
                if (aPrefix != bPrefix) return aPrefix;
                return a.second.name.length() < b.second.name.length();
            });
        
        return results;
    }
    
    /**
     * Get symbols in a specific file.
     */
    std::vector<LspDocumentSymbol> GetFileSymbols(const wxString& filePath) const {
        std::vector<LspDocumentSymbol> results;
        for (const auto& [path, symbol] : m_allSymbols) {
            if (path == filePath) {
                results.push_back(symbol);
            }
        }
        return results;
    }
    
    /**
     * Get symbols by kind (e.g., all functions, all classes).
     */
    std::vector<std::pair<wxString, LspDocumentSymbol>> GetSymbolsByKind(LspSymbolKind kind) const {
        std::vector<std::pair<wxString, LspDocumentSymbol>> results;
        for (const auto& [filePath, symbol] : m_allSymbols) {
            if (symbol.kind == kind) {
                results.push_back({filePath, symbol});
            }
        }
        return results;
    }
    
    /**
     * Check if indexing is complete.
     */
    bool IsIndexingComplete() const {
        return m_indexingComplete;
    }
    
    /**
     * Get the number of indexed files.
     */
    size_t GetIndexedFileCount() const {
        return m_indexedFiles.size();
    }
    
    /**
     * Get the number of indexed symbols.
     */
    size_t GetIndexedSymbolCount() const {
        return m_allSymbols.size();
    }
    
    /**
     * Get the LSP client for direct access if needed.
     */
    LspClient* GetLspClient() { return m_lspClient.get(); }
    
    /**
     * Check if currently operating in remote/SSH mode.
     */
    bool IsRemoteMode() const {
        return m_isRemoteMode;
    }
    
    /**
     * Reinitialize the widget when SSH connection state changes.
     * This recreates the LSP client with appropriate SSH config
     * and re-scans the workspace.
     */
    void Reinitialize() {
        auto& config = Config::Instance();
        bool sshEnabled = config.GetBool("ssh.enabled", false);
        
        wxLogMessage("=== SymbolsWidget::Reinitialize ===");
        wxLogMessage("SymbolsWidget: SSH enabled in config: %s", sshEnabled ? "YES" : "NO");
        wxLogMessage("SymbolsWidget: Current m_isRemoteMode: %s", m_isRemoteMode ? "YES" : "NO");
        
        // Reset initialization guard for new reinit
        m_isInitializing = false;
        
        // Stop any ongoing indexing
        wxLogMessage("SymbolsWidget: Stopping indexing");
        StopIndexing();
        
        // Stop existing LSP client
        if (m_lspClient) {
            wxLogMessage("SymbolsWidget: Stopping existing LSP client");
            m_lspClient->stop();
            m_lspClient.reset();
        }
        
        // Update workspace root based on mode
        m_isRemoteMode = sshEnabled;
        
        if (sshEnabled) {
            wxString remotePath = config.GetString("ssh.remotePath", "~");
            wxLogMessage("SymbolsWidget: Remote path from config: %s", remotePath);
            auto sshConfig = FS::SshConfig::LoadFromConfig();
            wxLogMessage("SymbolsWidget: SSH config loaded - host: %s, user: %s", 
                wxString::FromUTF8(sshConfig.host), wxString::FromUTF8(sshConfig.user));
            m_workspaceRoot = wxString(sshConfig.expandRemotePath(remotePath.ToStdString()));
            wxLogMessage("SymbolsWidget: Expanded workspace root: %s", m_workspaceRoot);
        } else {
            auto* rootPtr = m_context ? m_context->Get<wxString>("workspaceRoot") : nullptr;
            m_workspaceRoot = rootPtr ? *rootPtr : wxGetCwd();
            wxLogMessage("SymbolsWidget: Local workspace root: %s", m_workspaceRoot);
        }
        
        // Clear previous data
        m_allSymbols.clear();
        m_indexedFiles.clear();
        m_filesToIndex.clear();
        m_currentIndexFile = 0;
        m_indexingComplete = false;
        
        // Update UI
        if (m_treeCtrl) {
            m_treeCtrl->DeleteAllItems();
            m_treeCtrl->AddRoot("Workspace");
        }
        
        // Update title to show mode
        if (m_titleLabel) {
            wxString title = m_isRemoteMode ? "CODE INDEX [SSH]" : "CODE INDEX";
            m_titleLabel->SetLabel(title);
        }
        
        ShowStatus(m_isRemoteMode ? "Reinitializing for SSH..." : "Reinitializing...");
        
        // Reinitialize LSP and start indexing
        InitializeLspClient();
    }

private:
    wxPanel* m_panel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;
    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxButton* m_refreshButton = nullptr;
    wxTextCtrl* m_searchCtrl = nullptr;
    WidgetContext* m_context = nullptr;
    
    std::unique_ptr<LspClient> m_lspClient;
    wxString m_workspaceRoot;
    bool m_isRemoteMode = false;
    bool m_isInitializing = false;  // Guard against re-entrant initialization
    bool m_destroyed = false;       // Flag to detect use-after-destroy in callbacks
    
    // Index data
    std::vector<std::pair<std::string, LspDocumentSymbol>> m_allSymbols;
    std::set<std::string> m_indexedFiles;
    std::vector<std::string> m_filesToIndex;
    size_t m_currentIndexFile = 0;
    bool m_indexingComplete = false;
    wxTimer* m_indexTimeoutTimer = nullptr;
    std::shared_ptr<std::atomic<bool>> m_currentRequestCompleted;
    
    // File extensions to index
    const std::set<wxString> m_sourceExtensions = {
        "cpp", "cxx", "cc", "c", "h", "hpp", "hxx",
        "py", "js", "ts", "jsx", "tsx",
        "rs", "go", "java", "rb", "swift"
    };
    
    /**
     * Load SSH configuration for LSP client from global settings.
     */
    static LspSshConfig LoadLspSshConfig() {
        auto& config = Config::Instance();
        LspSshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        // Remote clangd command - if empty, will auto-detect nix on remote
        // Examples: "clangd", "nix develop -c clangd", "nix run nixpkgs#clang-tools -- clangd"
        ssh.remoteCommand = config.GetString("ssh.clangdCommand", "").ToStdString();
        return ssh;
    }
    
    /**
     * Initialize the LSP client.
     * Applies SSH configuration if enabled for remote code indexing.
     */
    void InitializeLspClient() {
        wxLogMessage("=== SymbolsWidget::InitializeLspClient ===");
        
        // Guard against re-entrant initialization
        if (m_isInitializing) {
            wxLogMessage("SymbolsWidget: Already initializing, skipping duplicate call");
            return;
        }
        m_isInitializing = true;
        
        // Ensure clean state
        if (m_lspClient) {
            wxLogMessage("SymbolsWidget: Stopping existing LSP client");
            m_lspClient->stop();
            m_lspClient.reset();
        }
        
        m_lspClient = std::make_unique<LspClient>();
        
        // Set up log callback to see what's happening - show ALL messages now
        m_lspClient->setLogCallback([this](const std::string& message) {
            wxLogMessage("LSP: %s", wxString::FromUTF8(message));
            // Check wxTheApp is valid before using CallAfter (can be null during shutdown)
            if (wxTheApp && !m_destroyed) {
                wxTheApp->CallAfter([this, message]() {
                    if (m_destroyed) return;
                    // Show important messages in status (but not debug separator lines)
                    if ((message.find("Error") != std::string::npos ||
                         message.find("error") != std::string::npos ||
                         message.find("Failed") != std::string::npos ||
                         message.find("[stderr]") != std::string::npos) &&
                        message.find("===") == std::string::npos) {
                        ShowStatus(wxString::FromUTF8(message.substr(0, 80)));
                    }
                });
            }
        });
        
        // Use stored remote mode flag for consistency
        auto& config = Config::Instance();
        m_isRemoteMode = config.GetBool("ssh.enabled", false);
        wxString workspaceRoot = m_workspaceRoot;
        
        wxLogMessage("SymbolsWidget: Remote mode: %s", m_isRemoteMode ? "YES" : "NO");
        wxLogMessage("SymbolsWidget: Workspace root: %s", m_workspaceRoot);
        
        if (m_isRemoteMode) {
            LspSshConfig lspSsh = LoadLspSshConfig();
            wxLogMessage("SymbolsWidget: SSH config - enabled: %d, host: %s, user: %s, remoteCommand: %s",
                lspSsh.enabled, 
                wxString::FromUTF8(lspSsh.host),
                wxString::FromUTF8(lspSsh.user),
                wxString::FromUTF8(lspSsh.remoteCommand));
            
            if (!lspSsh.isValid()) {
                ShowStatus("Invalid SSH configuration");
                wxLogMessage("SymbolsWidget: SSH enabled but config is invalid");
                m_isInitializing = false;
                return;
            }
            m_lspClient->setSshConfig(lspSsh);
            wxLogMessage("SymbolsWidget: SSH config applied to LSP client");
        } else {
            wxLogMessage("SymbolsWidget: Using local mode with workspace: %s", m_workspaceRoot);
        }
        
        // Try to find clangd in order of preference:
        // 1. User-configured path
        // 2. In PATH (for nix develop or system install)
        // 3. Via nix run (for standalone execution)
        wxString clangdCommand = FindClangdCommand();
        wxLogMessage("SymbolsWidget: FindClangdCommand returned: '%s'", clangdCommand);
        
        if (clangdCommand.IsEmpty()) {
            ShowStatus("clangd not found - install LLVM or configure lsp.clangd.path");
            wxLogMessage("SymbolsWidget: Could not find clangd command");
            m_isInitializing = false;
            return;
        }
        
        wxString statusMsg = m_isRemoteMode 
            ? "Starting remote clangd..." 
            : "Starting clangd...";
        ShowStatus(statusMsg);
        
        wxLogMessage("SymbolsWidget: Calling m_lspClient->start('%s', '%s')", 
            clangdCommand, m_workspaceRoot);
        
        if (!m_lspClient->start(std::string(clangdCommand.mb_str()), std::string(m_workspaceRoot.mb_str()))) {
            ShowStatus(m_isRemoteMode ? "Failed to start remote clangd" : "Failed to start clangd");
            wxLogMessage("SymbolsWidget: m_lspClient->start() returned false!");
            m_isInitializing = false;
            return;
        }
        
        wxLogMessage("SymbolsWidget: LSP client started, calling initialize()");
        
        m_lspClient->initialize([this](bool success) {
            wxLogMessage("SymbolsWidget: Initialize callback - success: %d", success);
            wxTheApp->CallAfter([this, success]() {
                if (m_destroyed) return;  // Widget was destroyed, bail out
                m_isInitializing = false;  // Reset guard now that initialization is done
                if (success) {
                    ShowStatus("LSP ready, scanning...");
                    wxLogMessage("SymbolsWidget: LSP initialized successfully, starting indexing");
                    StartIndexing();
                } else {
                    ShowStatus("LSP init failed - check logs");
                    wxLogMessage("SymbolsWidget: LSP initialization failed!");
                }
            });
        });
    }
    
    /**
     * Find the clangd command to use.
     * Tries multiple strategies to locate clangd.
     */
    wxString FindClangdCommand() {
        // 1. Check user config
        wxString configured = Config::Instance().GetString("lsp.clangd.path", "");
        if (!configured.IsEmpty()) {
            return configured;
        }
        
        // Handle Remote Mode
        if (m_isRemoteMode) {
            auto sshConfig = FS::SshConfig::LoadFromConfig();
            if (sshConfig.isValid()) {
                wxString sshPrefix = wxString::FromUTF8(sshConfig.buildSshPrefix());
                wxArrayString output, errors;
                
                // 2. Check if clangd is in remote PATH
                wxString cmd = sshPrefix + " \"which clangd\"";
                if (wxExecute(cmd, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE) == 0) {
                    return "clangd";
                }
                
                // 3. Try remote nix run
                cmd = sshPrefix + " \"which nix\"";
                if (wxExecute(cmd, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE) == 0) {
                    return "nix run nixpkgs#clang-tools -- clangd";
                }
            }
            return wxEmptyString;
        }
        
        // Local Mode Logic
        
        // 2. Check if clangd is in PATH
        wxArrayString output, errors;
        if (wxExecute("which clangd", output, errors, wxEXEC_SYNC) == 0 && !output.IsEmpty()) {
            return "clangd";
        }
        
        // 3. Check common install locations
        wxArrayString commonPaths = {
            "/usr/bin/clangd",
            "/usr/local/bin/clangd",
            "/opt/homebrew/bin/clangd",  // Homebrew on Apple Silicon
            "/usr/local/opt/llvm/bin/clangd"  // Homebrew LLVM
        };
        for (const auto& path : commonPaths) {
            if (wxFileExists(path)) {
                return path;
            }
        }
        
        // 4. Try nix run (works without being in nix develop)
        if (wxExecute("which nix", output, errors, wxEXEC_SYNC) == 0) {
            // Use nix run to invoke clangd from nixpkgs
            return "nix run nixpkgs#clang-tools -- clangd";
        }
        
        return wxEmptyString;
    }
    
    /**
     * Stop any ongoing indexing operation.
     */
    void StopIndexing() {
        // Cancel current request if pending
        if (m_currentRequestCompleted) {
            m_currentRequestCompleted->store(true);
        }
        
        // Stop timeout timer
        if (m_indexTimeoutTimer && m_indexTimeoutTimer->IsRunning()) {
            m_indexTimeoutTimer->Stop();
        }
        
        m_indexingComplete = true;  // Prevent further indexing
        ShowStatus("Indexing stopped");
    }
    
    /**
     * Start indexing the workspace.
     */
    void StartIndexing() {
        m_allSymbols.clear();
        m_indexedFiles.clear();
        m_filesToIndex.clear();
        m_currentIndexFile = 0;
        m_indexingComplete = false;
        
        // Scan for source files (supports both local and remote)
        ShowStatus(m_isRemoteMode ? "Scanning remote files..." : "Scanning files...");
        
        if (m_isRemoteMode) {
            ScanDirectoryRemote(m_workspaceRoot);
        } else {
            ScanDirectoryLocal(m_workspaceRoot);
        }
        
        if (m_filesToIndex.empty()) {
            ShowStatus("No source files found");
            m_indexingComplete = true;
            return;
        }
        
        ShowStatus(wxString::Format("Found %zu files, indexing...", m_filesToIndex.size()));
        
        // Start indexing files one by one
        IndexNextFile();
    }
    
    /**
     * Recursively scan local directory for source files.
     */
    void ScanDirectoryLocal(const wxString& dirPath) {
        wxDir dir(dirPath);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        
        // Scan files
        bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
        while (cont) {
            // wxDir::GetFirst/GetNext should return just the filename, but validate
            if (filename.Contains('/') || filename.Contains('\\')) {
                wxLogMessage("SymbolsWidget: Unexpected path in filename: %s", filename);
                cont = dir.GetNext(&filename);
                continue;
            }
            
            wxString ext = filename.AfterLast('.').Lower();
            if (m_sourceExtensions.count(ext)) {
                // Use path concatenation instead of wxFileName to avoid assert
                wxString fullPath = dirPath;
                if (!fullPath.EndsWith('/') && !fullPath.EndsWith('\\')) {
                    fullPath += wxFileName::GetPathSeparator();
                }
                fullPath += filename;
                m_filesToIndex.push_back(std::string(fullPath.ToUTF8().data()));
            }
            cont = dir.GetNext(&filename);
        }
        
        // Scan subdirectories (skip hidden and common non-source dirs)
        cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
        while (cont) {
            // Validate directory name doesn't contain path separators
            if (filename.Contains('/') || filename.Contains('\\')) {
                wxLogMessage("SymbolsWidget: Unexpected path in dirname: %s", filename);
                cont = dir.GetNext(&filename);
                continue;
            }
            
            if (ShouldScanDirectory(filename)) {
                // Use path concatenation instead of wxFileName to avoid assert
                wxString subDir = dirPath;
                if (!subDir.EndsWith('/') && !subDir.EndsWith('\\')) {
                    subDir += wxFileName::GetPathSeparator();
                }
                subDir += filename;
                ScanDirectoryLocal(subDir);
            }
            cont = dir.GetNext(&filename);
        }
    }
    
    /**
     * Recursively scan remote directory for source files via SSH.
     */
    void ScanDirectoryRemote(const wxString& dirPath, int depth = 0) {
        // Limit recursion depth to avoid very deep scans
        if (depth > 10) {
            wxLogMessage("SymbolsWidget: Max scan depth reached at %s", dirPath);
            return;
        }
        
        auto sshConfig = FS::SshConfig::LoadFromConfig();
        if (!sshConfig.isValid()) {
            wxLogMessage("SymbolsWidget: Invalid SSH config for remote scanning");
            return;
        }
        
        // Use find command for efficient remote scanning
        // This is much faster than recursive ls for large directories
        std::string sshPrefix = sshConfig.buildSshPrefix();
        std::string escapedPath = dirPath.ToStdString();
        
        // Build find command to get files and directories
        // Exclude common non-source directories and hidden files
        std::string cmd = sshPrefix + " \"find '" + escapedPath + "' -maxdepth 1 \\( -type f -o -type d \\) 2>/dev/null\" 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            wxLogMessage("SymbolsWidget: Failed to execute SSH find command");
            return;
        }
        
        std::vector<std::string> files;
        std::vector<std::string> directories;
        
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            // Remove trailing newline
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            
            if (line.empty() || line == dirPath.ToStdString()) continue;
            
            // Get just the filename
            std::string filename = line;
            size_t lastSlash = line.rfind('/');
            if (lastSlash != std::string::npos) {
                filename = line.substr(lastSlash + 1);
            }
            
            // Skip hidden files/directories
            if (!filename.empty() && filename[0] == '.') continue;
            
            // Check if it's a file or directory by testing extension
            wxString ext = wxString(filename).AfterLast('.').Lower();
            if (m_sourceExtensions.count(ext)) {
                files.push_back(line);
            } else if (filename.find('.') == std::string::npos || ext.length() > 10) {
                // Likely a directory (no extension or very long "extension")
                // We'll verify with stat below
                directories.push_back(line);
            }
        }
        pclose(pipe);
        
        // Add source files
        for (const auto& file : files) {
            m_filesToIndex.push_back(file);
        }
        
        // Recursively scan subdirectories
        for (const auto& dir : directories) {
            wxString dirName = wxString(dir).AfterLast('/');
            if (ShouldScanDirectory(dirName)) {
                // Verify it's actually a directory via SSH
                std::string testCmd = sshPrefix + " \"test -d '" + dir + "' && echo yes\" 2>/dev/null";
                FILE* testPipe = popen(testCmd.c_str(), "r");
                if (testPipe) {
                    char testBuf[16];
                    bool isDir = (fgets(testBuf, sizeof(testBuf), testPipe) != nullptr);
                    pclose(testPipe);
                    if (isDir) {
                        ScanDirectoryRemote(wxString(dir), depth + 1);
                    }
                }
            }
        }
    }
    
    /**
     * Check if a directory should be scanned (not excluded).
     */
    bool ShouldScanDirectory(const wxString& dirname) const {
        return !dirname.StartsWith(".") && 
               dirname != "node_modules" && 
               dirname != "build" && 
               dirname != "target" &&
               dirname != "__pycache__" &&
               dirname != "venv" &&
               dirname != ".git" &&
               dirname != "dist" &&
               dirname != ".cache";
    }
    
    /**
     * Recursively scan directory for source files.
     * @deprecated Use ScanDirectoryLocal or ScanDirectoryRemote instead.
     */
    void ScanDirectory(const wxString& dirPath) {
        // For backward compatibility, delegate to appropriate method
        if (m_isRemoteMode) {
            ScanDirectoryRemote(dirPath);
        } else {
            ScanDirectoryLocal(dirPath);
        }
    }
    
    /**
     * Index the next file in the queue.
     */
    void IndexNextFile() {
        // Safety check - ensure panel is still valid
        if (!m_panel || !m_panel->IsShown()) {
            wxLogMessage("SymbolsWidget: Panel invalid or hidden, stopping indexing");
            m_indexingComplete = true;
            return;
        }
        
        if (m_currentIndexFile >= m_filesToIndex.size()) {
            // Indexing complete
            m_indexingComplete = true;
            ShowStatus(wxString::Format("Indexed %zu symbols in %zu files", 
                m_allSymbols.size(), m_indexedFiles.size()));
            RebuildTree();
            return;
        }
        
        // Check if we've been told to stop
        if (m_indexingComplete) {
            return;
        }
        
        wxString filePath = m_filesToIndex[m_currentIndexFile];
        wxString uri = pathToUri(std::string(filePath.mb_str()));
        
        // Update status
        wxString fileName = filePath.AfterLast('/');
        if (fileName.IsEmpty()) fileName = filePath.AfterLast('\\');
        if (fileName.IsEmpty()) fileName = filePath;
        
        ShowStatus(wxString::Format("Indexing %zu/%zu: %s", 
            m_currentIndexFile + 1, m_filesToIndex.size(), fileName));
        
        // Read file content (local or remote)
        wxString content;
        bool readSuccess = false;
        
        if (m_isRemoteMode) {
            readSuccess = ReadRemoteFile(filePath, content);
        } else {
            readSuccess = ReadLocalFile(filePath, content);
        }
        
        if (!readSuccess || content.IsEmpty()) {
            wxLogMessage("SymbolsWidget: Failed to read file %s, skipping", filePath);
            m_currentIndexFile++;
            // Use CallAfter to prevent stack overflow on many failures
            wxTheApp->CallAfter([this]() {
                if (m_destroyed) return;
                IndexNextFile();
            });
            return;
        }
        
        // Notify LSP about the file
        wxString langId = DetectLanguage(filePath);
        m_lspClient->didOpen(
            std::string(uri.mb_str()),
            std::string(langId.mb_str()),
            std::string(content.mb_str())
        );
        
        wxLogMessage("LSP: Requesting symbols from %s", wxString::FromUTF8(filePath.c_str()));
        
        // Request symbols with timeout protection
        m_currentRequestCompleted = std::make_shared<std::atomic<bool>>(false);
        auto requestCompleted = m_currentRequestCompleted;
        
        m_lspClient->getDocumentSymbols(std::string(uri.mb_str()), [this, filePath, uri, requestCompleted](const std::vector<LspDocumentSymbol>& symbols) {
            if (requestCompleted->exchange(true)) {
                wxLogMessage("LSP: Callback fired but request already completed for %s", wxString::FromUTF8(filePath.c_str()));
                return; // Already handled by timeout
            }
            
            wxTheApp->CallAfter([this, filePath, uri, symbols]() {
                if (m_destroyed) return;
                
                // Stop timeout timer
                if (m_indexTimeoutTimer && m_indexTimeoutTimer->IsRunning()) {
                    m_indexTimeoutTimer->Stop();
                }
                
                wxLogMessage("LSP: Received %zu symbols from %s", symbols.size(), wxString::FromUTF8(filePath.c_str()));
                
                // Store symbols
                CollectSymbols(filePath, symbols);
                m_indexedFiles.insert(std::string(filePath.ToUTF8().data()));
                
                // Close the document to free LSP memory
                m_lspClient->didClose(std::string(uri.mb_str()));
                
                // Continue to next file
                m_currentIndexFile++;
                IndexNextFile();
            });
        });
        
        // Set a timeout in case clangd doesn't respond
        if (!m_indexTimeoutTimer) {
            m_indexTimeoutTimer = new wxTimer(m_panel);
            m_panel->Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
                if (m_destroyed) return;
                
                // Use member variable that was captured before timeout
                auto requestCompleted = m_currentRequestCompleted;
                if (!requestCompleted || requestCompleted->exchange(true)) {
                    wxLogMessage("LSP: Timeout fired but request already completed");
                    return;
                }
                
                wxLogMessage("LSP: Timeout (5s) waiting for symbols, skipping file %zu/%zu", 
                    m_currentIndexFile + 1, m_filesToIndex.size());
                
                // Close the current document
                if (m_currentIndexFile < m_filesToIndex.size()) {
                    wxString currentFilePath = m_filesToIndex[m_currentIndexFile];
                    wxString currentUri = pathToUri(std::string(currentFilePath.mb_str()));
                    m_lspClient->didClose(std::string(currentUri.mb_str()));
                }
                
                // Continue to next file
                m_currentIndexFile++;
                IndexNextFile();
            });
        }
        m_indexTimeoutTimer->StartOnce(5000); // 5 second timeout
    }
    
    /**
     * Recursively collect symbols from hierarchical structure.
     */
    void CollectSymbols(const wxString& filePath, const std::vector<LspDocumentSymbol>& symbols) {
        for (const auto& symbol : symbols) {
            m_allSymbols.push_back({std::string(filePath.ToUTF8().data()), symbol});
            
            // Collect children
            if (!symbol.children.empty()) {
                CollectSymbols(filePath, symbol.children);
            }
        }
    }
    
    /**
     * Rebuild the tree view with current symbols.
     */
    void RebuildTree(const std::string& filter = "") {
        m_treeCtrl->Freeze();
        m_treeCtrl->DeleteAllItems();
        wxTreeItemId root = m_treeCtrl->AddRoot("Workspace");
        
        std::string lowerFilter = filter;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
        
        // Group symbols by file
        std::map<std::string, std::vector<const LspDocumentSymbol*>> fileSymbols;
        
        for (const auto& [filePath, symbol] : m_allSymbols) {
            // Apply filter
            std::string lowerName = symbol.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (!lowerFilter.empty() && lowerName.find(lowerFilter) == std::string::npos) {
                continue;
            }
            fileSymbols[filePath].push_back(&symbol);
        }
        
        // Build tree
        for (const auto& [filePath, symbols] : fileSymbols) {
            wxString relativePath = filePath;
            if (relativePath.StartsWith(m_workspaceRoot)) {
                relativePath = relativePath.Mid(m_workspaceRoot.length());
                if (relativePath.StartsWith("/") || relativePath.StartsWith("\\")) {
                    relativePath = relativePath.Mid(1);
                }
            }
            
            wxTreeItemId fileItem = m_treeCtrl->AppendItem(root, 
                wxString::FromUTF8("📄 ") + relativePath, 
                -1, -1, new SymbolData(filePath));
            
            for (const auto* symbol : symbols) {
                wxString icon = getSymbolKindIcon(symbol->kind);
                wxString label = icon + " " + symbol->name;
                if (!symbol->detail.empty()) {
                    label += " : " + symbol->detail;
                }
                m_treeCtrl->AppendItem(fileItem, label, -1, -1, 
                    new SymbolData(filePath, *symbol));
            }
            
            // Expand file node if filtering
            if (!lowerFilter.empty()) {
                m_treeCtrl->Expand(fileItem);
            }
        }
        
        m_treeCtrl->Thaw();
        
        // Expand all if filtering
        if (!lowerFilter.empty()) {
            m_treeCtrl->ExpandAll();
        }
    }
    
    /**
     * Read a local file's content.
     */
    bool ReadLocalFile(const wxString& filePath, wxString& content) {
        wxFile file(filePath);
        if (!file.IsOpened()) {
            return false;
        }
        
        bool success = file.ReadAll(&content);
        file.Close();
        return success && !content.IsEmpty();
    }
    
    /**
     * Read a remote file's content via SSH.
     */
    bool ReadRemoteFile(const wxString& filePath, wxString& content) {
        auto sshConfig = FS::SshConfig::LoadFromConfig();
        if (!sshConfig.isValid()) {
            wxLogMessage("SymbolsWidget: Invalid SSH config for reading remote file");
            return false;
        }
        
        std::string sshPrefix = sshConfig.buildSshPrefix();
        std::string escapedPath = filePath.ToStdString();
        
        // Use cat to read file content
        std::string cmd = sshPrefix + " \"cat '" + escapedPath + "'\" 2>/dev/null";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            wxLogMessage("SymbolsWidget: Failed to execute SSH cat command for %s", filePath);
            return false;
        }
        
        std::string result;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        int status = pclose(pipe);
        if (status != 0) {
            wxLogMessage("SymbolsWidget: SSH cat command failed with status %d for %s", status, filePath);
            return false;
        }
        
        content = wxString::FromUTF8(result.c_str());
        return !content.IsEmpty();
    }
    
    /**
     * Show status message.
     */
    void ShowStatus(const wxString& message) {
        if (m_statusLabel) {
            m_statusLabel->SetLabel(message);
        }
    }
    
    /**
     * Detect language ID from file extension.
     */
    wxString DetectLanguage(const wxString& filePath) {
        wxString ext = filePath.AfterLast('.').Lower();
        
        if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c++") return "cpp";
        if (ext == "c") return "c";
        if (ext == "h" || ext == "hpp" || ext == "hxx") return "cpp";
        if (ext == "py") return "python";
        if (ext == "js") return "javascript";
        if (ext == "ts") return "typescript";
        if (ext == "jsx") return "javascriptreact";
        if (ext == "tsx") return "typescriptreact";
        if (ext == "rs") return "rust";
        if (ext == "go") return "go";
        if (ext == "java") return "java";
        if (ext == "rb") return "ruby";
        if (ext == "swift") return "swift";
        
        return "plaintext";
    }
    
    // Event handlers
    
    void OnItemActivated(wxTreeEvent& event) {
        wxTreeItemId itemId = event.GetItem();
        if (!itemId.IsOk()) return;
        
        SymbolData* data = dynamic_cast<SymbolData*>(m_treeCtrl->GetItemData(itemId));
        if (!data) return;
        
        // Get editor
        auto* editor = m_context ? m_context->Get<Editor>("editorComponent") : nullptr;
        if (!editor) return;
        
        // Open file if not already open
        wxString filePath = data->GetFilePath();
        if (editor->GetFilePath() != filePath) {
            // Use remote or local file opening based on mode
            if (m_isRemoteMode) {
                auto sshConfig = FS::SshConfig::LoadFromConfig();
                editor->OpenRemoteFile(filePath, sshConfig.buildSshPrefix());
            } else {
                editor->OpenFile(filePath);
            }
        }
        
        // Navigate to symbol position if it's a symbol (not just a file)
        if (!data->IsFile()) {
            const LspDocumentSymbol& symbol = data->GetSymbol();
            wxStyledTextCtrl* textCtrl = editor->GetTextCtrl();
            if (textCtrl) {
                int line = symbol.selectionRange.start.line;
                int col = symbol.selectionRange.start.character;
                int pos = textCtrl->PositionFromLine(line) + col;
                
                textCtrl->GotoPos(pos);
                textCtrl->EnsureCaretVisible();
                textCtrl->SetFocus();
                
                // Select the symbol name
                int endLine = symbol.selectionRange.end.line;
                int endCol = symbol.selectionRange.end.character;
                int endPos = textCtrl->PositionFromLine(endLine) + endCol;
                textCtrl->SetSelection(pos, endPos);
            }
        }
    }
    
    void OnRefreshClicked(wxCommandEvent& event) {
        StartIndexing();
    }
    
    void OnSearchTextChanged(wxCommandEvent& event) {
        // Rebuild tree with filter as user types
        if (m_indexingComplete) {
            RebuildTree(std::string(m_searchCtrl->GetValue().ToUTF8().data()));
        }
    }
    
    void OnSearch(wxCommandEvent& event) {
        // Same as text changed, but also focus tree
        if (m_indexingComplete) {
            RebuildTree(std::string(m_searchCtrl->GetValue().ToUTF8().data()));
            m_treeCtrl->SetFocus();
        }
    }
};

} // namespace BuiltinWidgets

#endif // SYMBOLS_WIDGET_H
