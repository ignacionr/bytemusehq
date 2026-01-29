#ifndef SYMBOLS_WIDGET_H
#define SYMBOLS_WIDGET_H

#include "widget.h"
#include "editor.h"
#include "../lsp/lsp_client.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/srchctrl.h>
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
        
        // Search box
        m_searchCtrl = new wxSearchCtrl(m_panel, wxID_ANY, "", 
            wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        m_searchCtrl->SetDescriptiveText("Search symbols...");
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
        
        // Get workspace root
        auto* rootPtr = context.Get<wxString>("workspaceRoot");
        m_workspaceRoot = rootPtr ? *rootPtr : wxGetCwd();
        
        // Bind events
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &SymbolsWidget::OnItemActivated, this);
        m_refreshButton->Bind(wxEVT_BUTTON, &SymbolsWidget::OnRefreshClicked, this);
        m_searchCtrl->Bind(wxEVT_TEXT, &SymbolsWidget::OnSearchTextChanged, this);
        m_searchCtrl->Bind(wxEVT_SEARCHCTRL_SEARCH_BTN, &SymbolsWidget::OnSearch, this);
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
        
        m_searchCtrl->SetBackgroundColour(theme->palette.inputBackground);
        m_searchCtrl->SetForegroundColour(theme->palette.inputForeground);
        
        m_refreshButton->SetBackgroundColour(theme->ui.sidebarBackground);
        m_refreshButton->SetForegroundColour(theme->ui.sidebarForeground);
        
        m_panel->Refresh();
    }

    void OnShow(wxWindow* window, WidgetContext& context) override {
        // Trigger indexing if not already done
        if (m_allSymbols.empty() && m_lspClient && m_lspClient->IsInitialized()) {
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
    const std::vector<std::pair<wxString, LspDocumentSymbol>>& GetAllSymbols() const {
        return m_allSymbols;
    }
    
    /**
     * Search symbols by name (fuzzy match).
     */
    std::vector<std::pair<wxString, LspDocumentSymbol>> SearchSymbols(const wxString& query) const {
        std::vector<std::pair<wxString, LspDocumentSymbol>> results;
        wxString lowerQuery = query.Lower();
        
        for (const auto& [filePath, symbol] : m_allSymbols) {
            if (symbol.name.Lower().Contains(lowerQuery)) {
                results.push_back({filePath, symbol});
            }
        }
        
        // Sort by relevance (exact prefix match first, then contains)
        std::sort(results.begin(), results.end(), 
            [&lowerQuery](const auto& a, const auto& b) {
                bool aPrefix = a.second.name.Lower().StartsWith(lowerQuery);
                bool bPrefix = b.second.name.Lower().StartsWith(lowerQuery);
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

private:
    wxPanel* m_panel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;
    wxStaticText* m_titleLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxButton* m_refreshButton = nullptr;
    wxSearchCtrl* m_searchCtrl = nullptr;
    WidgetContext* m_context = nullptr;
    
    std::unique_ptr<LspClient> m_lspClient;
    wxString m_workspaceRoot;
    
    // Index data
    std::vector<std::pair<wxString, LspDocumentSymbol>> m_allSymbols;
    std::set<wxString> m_indexedFiles;
    std::vector<wxString> m_filesToIndex;
    size_t m_currentIndexFile = 0;
    bool m_indexingComplete = false;
    
    // File extensions to index
    const std::set<wxString> m_sourceExtensions = {
        "cpp", "cxx", "cc", "c", "h", "hpp", "hxx",
        "py", "js", "ts", "jsx", "tsx",
        "rs", "go", "java", "rb", "swift"
    };
    
    /**
     * Initialize the LSP client.
     */
    void InitializeLspClient() {
        m_lspClient = std::make_unique<LspClient>();
        
        // Try to find clangd in order of preference:
        // 1. User-configured path
        // 2. In PATH (for nix develop or system install)
        // 3. Via nix run (for standalone execution)
        wxString clangdCommand = FindClangdCommand();
        
        if (clangdCommand.IsEmpty()) {
            ShowStatus("clangd not found");
            return;
        }
        
        ShowStatus("Starting clangd...");
        
        if (!m_lspClient->Start(clangdCommand, m_workspaceRoot)) {
            ShowStatus("Failed to start clangd");
            return;
        }
        
        m_lspClient->Initialize([this](bool success) {
            wxTheApp->CallAfter([this, success]() {
                if (success) {
                    ShowStatus("LSP ready, scanning...");
                    StartIndexing();
                } else {
                    ShowStatus("LSP init failed");
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
     * Start indexing the workspace.
     */
    void StartIndexing() {
        m_allSymbols.clear();
        m_indexedFiles.clear();
        m_filesToIndex.clear();
        m_currentIndexFile = 0;
        m_indexingComplete = false;
        
        // Scan for source files
        ScanDirectory(m_workspaceRoot);
        
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
     * Recursively scan directory for source files.
     */
    void ScanDirectory(const wxString& dirPath) {
        wxDir dir(dirPath);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        
        // Scan files
        bool cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
        while (cont) {
            wxString ext = filename.AfterLast('.').Lower();
            if (m_sourceExtensions.count(ext)) {
                wxString fullPath = wxFileName(dirPath, filename).GetFullPath();
                m_filesToIndex.push_back(fullPath);
            }
            cont = dir.GetNext(&filename);
        }
        
        // Scan subdirectories (skip hidden and common non-source dirs)
        cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
        while (cont) {
            if (!filename.StartsWith(".") && 
                filename != "node_modules" && 
                filename != "build" && 
                filename != "target" &&
                filename != "__pycache__" &&
                filename != "venv" &&
                filename != ".git") {
                wxString subDir = wxFileName(dirPath, filename).GetFullPath();
                ScanDirectory(subDir);
            }
            cont = dir.GetNext(&filename);
        }
    }
    
    /**
     * Index the next file in the queue.
     */
    void IndexNextFile() {
        if (m_currentIndexFile >= m_filesToIndex.size()) {
            // Indexing complete
            m_indexingComplete = true;
            ShowStatus(wxString::Format("Indexed %zu symbols in %zu files", 
                m_allSymbols.size(), m_indexedFiles.size()));
            RebuildTree();
            return;
        }
        
        wxString filePath = m_filesToIndex[m_currentIndexFile];
        wxString uri = PathToUri(filePath);
        
        // Update status
        wxString fileName = wxFileName(filePath).GetFullName();
        ShowStatus(wxString::Format("Indexing %zu/%zu: %s", 
            m_currentIndexFile + 1, m_filesToIndex.size(), fileName));
        
        // Read file content
        wxFile file(filePath);
        if (!file.IsOpened()) {
            m_currentIndexFile++;
            IndexNextFile();
            return;
        }
        
        wxString content;
        file.ReadAll(&content);
        file.Close();
        
        // Notify LSP about the file
        wxString langId = DetectLanguage(filePath);
        m_lspClient->DidOpen(uri, langId, content);
        
        // Request symbols
        m_lspClient->GetDocumentSymbols(uri, [this, filePath, uri](const std::vector<LspDocumentSymbol>& symbols) {
            wxTheApp->CallAfter([this, filePath, uri, symbols]() {
                // Store symbols
                CollectSymbols(filePath, symbols);
                m_indexedFiles.insert(filePath);
                
                // Close the document to free LSP memory
                m_lspClient->DidClose(uri);
                
                // Continue to next file
                m_currentIndexFile++;
                IndexNextFile();
            });
        });
    }
    
    /**
     * Recursively collect symbols from hierarchical structure.
     */
    void CollectSymbols(const wxString& filePath, const std::vector<LspDocumentSymbol>& symbols) {
        for (const auto& symbol : symbols) {
            m_allSymbols.push_back({filePath, symbol});
            
            // Collect children
            if (!symbol.children.empty()) {
                CollectSymbols(filePath, symbol.children);
            }
        }
    }
    
    /**
     * Rebuild the tree view with current symbols.
     */
    void RebuildTree(const wxString& filter = "") {
        m_treeCtrl->Freeze();
        m_treeCtrl->DeleteAllItems();
        wxTreeItemId root = m_treeCtrl->AddRoot("Workspace");
        
        wxString lowerFilter = filter.Lower();
        
        // Group symbols by file
        std::map<wxString, std::vector<const LspDocumentSymbol*>> fileSymbols;
        
        for (const auto& [filePath, symbol] : m_allSymbols) {
            // Apply filter
            if (!lowerFilter.IsEmpty() && !symbol.name.Lower().Contains(lowerFilter)) {
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
                wxString icon = GetSymbolKindIcon(symbol->kind);
                wxString label = icon + " " + symbol->name;
                if (!symbol->detail.IsEmpty()) {
                    label += " : " + symbol->detail;
                }
                m_treeCtrl->AppendItem(fileItem, label, -1, -1, 
                    new SymbolData(filePath, *symbol));
            }
            
            // Expand file node if filtering
            if (!lowerFilter.IsEmpty()) {
                m_treeCtrl->Expand(fileItem);
            }
        }
        
        m_treeCtrl->Thaw();
        
        // Expand all if filtering
        if (!lowerFilter.IsEmpty()) {
            m_treeCtrl->ExpandAll();
        }
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
            editor->OpenFile(filePath);
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
            RebuildTree(m_searchCtrl->GetValue());
        }
    }
    
    void OnSearch(wxCommandEvent& event) {
        // Same as text changed, but also focus tree
        if (m_indexingComplete) {
            RebuildTree(m_searchCtrl->GetValue());
            m_treeCtrl->SetFocus();
        }
    }
};

} // namespace BuiltinWidgets

#endif // SYMBOLS_WIDGET_H
