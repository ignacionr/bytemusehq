#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H

#include <wx/wx.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <functional>
#include <map>
#include <queue>
#include <memory>
#include <sstream>
// Use Glaze for JSON parsing instead of ad-hoc parser
#include <glaze/glaze.hpp>

using JsonValue = glz::generic;

/**
 * LSP (Language Server Protocol) client for ByteMuseHQ.
 * 
 * Communicates with language servers (e.g., clangd) via JSON-RPC over stdin/stdout.
 * Provides code intelligence features like go-to-definition, find references,
 * document symbols, and diagnostics.
 */

// Forward declarations
class LspClient;

// ============================================================================
// LSP Data Types
// ============================================================================

struct LspPosition {
    int line = 0;      // 0-based line number
    int character = 0; // 0-based character offset
};

struct LspRange {
    LspPosition start;
    LspPosition end;
};

struct LspLocation {
    std::string uri;
    LspRange range;
};

// Glaze metadata for LSP types (used for parsing with glaze::read)
// Move these to global namespace at the end of the file
enum class LspSymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

struct LspDocumentSymbol {
    std::string name;
    std::string detail;
    LspSymbolKind kind;
    LspRange range;
    LspRange selectionRange;
    std::vector<LspDocumentSymbol> children;
};

struct LspDiagnostic {
    LspRange range;
    int severity; // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string code;
    std::string source;
    std::string message;
};



struct LspCompletionItem {
    std::string label;
    int kind;
    std::string detail;
    std::string documentation;
    std::string insertText;
};

template<> struct glz::meta<LspPosition> {
    using T = LspPosition;
    static constexpr auto value = object("line", &T::line, "character", &T::character);
};

template<> struct glz::meta<LspRange> {
    using T = LspRange;
    static constexpr auto value = object("start", &T::start, "end", &T::end);
};

template<> struct glz::meta<LspLocation> {
    using T = LspLocation;
    static constexpr auto value = object("uri", &T::uri, "range", &T::range);
};

template<> struct glz::meta<LspDocumentSymbol> {
    using T = LspDocumentSymbol;
    static constexpr auto value = object("name", &T::name, "detail", &T::detail, "kind", &T::kind, "range", &T::range, "selectionRange", &T::selectionRange, "children", &T::children);
};

template<> struct glz::meta<LspDiagnostic> {
    using T = LspDiagnostic;
    static constexpr auto value = object("range", &T::range, "severity", &T::severity, "code", &T::code, "source", &T::source, "message", &T::message);
};

template<> struct glz::meta<LspCompletionItem> {
    using T = LspCompletionItem;
    static constexpr auto value = object("label", &T::label, "kind", &T::kind, "detail", &T::detail, "documentation", &T::documentation, "insertText", &T::insertText);
};

// Glaze-friendly mirror types using std::string where needed
struct GzDiagnostic {
    LspRange range;
    int severity;
    std::string code;
    std::string source;
    std::string message;
};

template<> struct glz::meta<GzDiagnostic> {
    using T = GzDiagnostic;
    static constexpr auto value = object("range", &T::range, "severity", &T::severity, "code", &T::code, "source", &T::source, "message", &T::message);
};

struct DiagnosticsParams {
    std::string uri;
    std::vector<GzDiagnostic> diagnostics;
};

template<> struct glz::meta<DiagnosticsParams> {
    using T = DiagnosticsParams;
    static constexpr auto value = object("uri", &T::uri, "diagnostics", &T::diagnostics);
};


struct GzDocumentSymbol {
    std::string name;
    std::string detail;
    int kind;
    LspRange range;
    LspRange selectionRange;
    std::vector<GzDocumentSymbol> children;
};

template<> struct glz::meta<GzDocumentSymbol> {
    using T = GzDocumentSymbol;
    static constexpr auto value = object("name", &T::name, "detail", &T::detail, "kind", &T::kind, "range", &T::range, "selectionRange", &T::selectionRange, "children", &T::children);
};

struct GzLocation {
    std::string uri;
    LspRange range;
};

template<> struct glz::meta<GzLocation> {
    using T = GzLocation;
    static constexpr auto value = object("uri", &T::uri, "range", &T::range);
};

struct GzCompletionItem {
    std::string label;
    int kind;
    std::string detail;
    std::string documentation;
    std::string insertText;
};

template<> struct glz::meta<GzCompletionItem> {
    using T = GzCompletionItem;
    static constexpr auto value = object("label", &T::label, "kind", &T::kind, "detail", &T::detail, "documentation", &T::documentation, "insertText", &T::insertText);
};

// Helper converters from Gz* to LSP types
static LspDiagnostic fromGzDiagnostic(const GzDiagnostic& g) {
    LspDiagnostic d;
    d.severity = g.severity;
    d.code = g.code;
    d.source = g.source;
    d.message = g.message;
    d.range = g.range;
    return d;
}

static LspDocumentSymbol fromGzDocumentSymbol(const GzDocumentSymbol& g) {
    LspDocumentSymbol s;
    s.name = g.name;
    s.detail = g.detail;
    s.kind = static_cast<LspSymbolKind>(g.kind);
    s.range = g.range;
    s.selectionRange = g.selectionRange;
    for (const auto& c : g.children) s.children.push_back(fromGzDocumentSymbol(c));
    return s;
}

static LspLocation fromGzLocation(const GzLocation& g) {
    LspLocation l;
    l.uri = g.uri;
    l.range = g.range;
    return l;
}

static LspCompletionItem fromGzCompletionItem(const GzCompletionItem& g) {
    LspCompletionItem i;
    i.label = g.label;
    i.kind = g.kind;
    i.detail = g.detail;
    i.documentation = g.documentation;
    i.insertText = g.insertText;
    return i;
}

// ============================================================================
// Callbacks
// ============================================================================

using InitializeCallback = std::function<void(bool success)>;
using SymbolsCallback = std::function<void(const std::vector<LspDocumentSymbol>& symbols)>;
using LocationCallback = std::function<void(const std::vector<LspLocation>& locations)>;
using DiagnosticsCallback = std::function<void(const std::string& uri, const std::vector<LspDiagnostic>& diagnostics)>;
using CompletionCallback = std::function<void(const std::vector<LspCompletionItem>& items)>;

// ============================================================================
// SSH Configuration for Remote LSP
// ============================================================================

/**
 * SSH configuration for running language servers on remote machines.
 * When enabled, the LSP client will run the language server via SSH.
 */
struct LspSshConfig {
    bool enabled = false;
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    
    /**
     * Build SSH command prefix for running remote LSP server.
     */
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        
        std::string cmd = "ssh";
        
        // Use -tt for pseudo-terminal allocation (needed for stdin/stdout)
        cmd += " -tt";
        
        if (!extraOptions.empty()) {
            cmd += " " + extraOptions;
        }
        
        if (!identityFile.empty()) {
            cmd += " -i \"" + identityFile + "\"";
        }
        
        if (port != 22) {
            cmd += " -p " + std::to_string(port);
        }
        
        cmd += " -o ConnectTimeout=" + std::to_string(connectionTimeout);
        cmd += " -o StrictHostKeyChecking=accept-new";
        
        if (!user.empty()) {
            cmd += " " + user + "@" + host;
        } else {
            cmd += " " + host;
        }
        
        return cmd;
    }
    
    bool isValid() const {
        return enabled && !host.empty();
    }
};

// ============================================================================
// JSON Helper (minimal implementation)
// ============================================================================

/**
 * Minimal JSON builder for LSP messages.
 * For a production implementation, consider using nlohmann/json or rapidjson.
 */

// ============================================================================
// LSP Client
// ============================================================================

/**
 * Client for communicating with Language Server Protocol servers.
 * Supports both local and remote (SSH) language server execution.
 * 
 * Usage:
 * @code
 * LspClient client;
 * client.SetDiagnosticsCallback([](const wxString& uri, const auto& diags) {
 *     // Handle diagnostics
 * });
 * 
 * // For local development:
 * if (client.Start("clangd", "/path/to/project")) { ... }
 * 
 * // For remote development via SSH:
 * LspSshConfig ssh;
 * ssh.enabled = true;
 * ssh.host = "dev-machine";
 * ssh.user = "developer";
 * client.SetSshConfig(ssh);
 * if (client.Start("clangd", "/home/developer/project")) { ... }
 * @endcode
 */
class LspClient : public wxEvtHandler {
private:
    wxProcess* m_process = nullptr;
    wxTimer m_pollTimer;
    wxString m_workspaceRoot;
    int m_nextId = 1;
    bool m_initialized = false;
    LspSshConfig m_sshConfig;
    
public:
    LspClient() = default;
    
    ~LspClient() {
        Stop();
    }
    
    /**
     * Configure SSH for remote language server execution.
     */
    void SetSshConfig(const LspSshConfig& config) {
        m_sshConfig = config;
    }
    
    /**
     * Get current SSH configuration.
     */
    LspSshConfig GetSshConfig() const {
        return m_sshConfig;
    }
    
    /**
     * Check if remote execution is enabled.
     */
    bool IsRemoteExecution() const {
        return m_sshConfig.isValid();
    }
    
    /**
     * Start the language server process.
     * If SSH is configured, runs the server on the remote machine.
     * @param command The server command (e.g., "clangd" or "nix run nixpkgs#clang-tools -- clangd")
     * @param workspaceRoot The workspace root path (local path, or remote path if SSH enabled)
     * @return true if the process started successfully
     */
    bool Start(const wxString& command, const wxString& workspaceRoot) {
        if (m_process) {
            Stop();
        }
        
        m_workspaceRoot = workspaceRoot;
        m_process = new wxProcess(this);
        m_process->Redirect();
        
        // Build the full command with arguments
        wxString fullCommand;
        wxString lspCommand;
        
        if (command.Contains("nix run")) {
            // For nix run, --background-index goes after the -- separator
            lspCommand = command + " --background-index";
        } else {
            lspCommand = command + " --background-index";
        }
        
        // If SSH is configured, wrap the command
        if (m_sshConfig.isValid()) {
            wxString sshPrefix = wxString(m_sshConfig.buildSshPrefix());
            // Escape the LSP command for SSH
            wxString escapedCmd = lspCommand;
            escapedCmd.Replace("\"", "\\\"");
            // Change to workspace directory and run the LSP server
            fullCommand = sshPrefix + " \"cd '" + workspaceRoot + "' && " + escapedCmd + "\"";
        } else {
            fullCommand = lspCommand;
        }
        
        long pid = wxExecute(fullCommand, wxEXEC_ASYNC, m_process);
        if (pid <= 0) {
            delete m_process;
            m_process = nullptr;
            return false;
        }
        
        // Set up event handling for process output
        Bind(wxEVT_END_PROCESS, &LspClient::OnProcessTerminated, this);
        
        // Start a timer to poll for output
        Bind(wxEVT_TIMER, &LspClient::OnPollTimer, this, m_pollTimer.GetId());
        m_pollTimer.Start(50); // Poll every 50ms
        
        return true;
    }
    
    /**
     * Stop the language server process.
     */
    void Stop() {
        m_pollTimer.Stop();
        
        if (m_process) {
            // Send shutdown request
            if (m_initialized) {
                SendRequest("shutdown", "null");
                SendNotification("exit", "null");
            }
            
            wxProcess::Kill(m_process->GetPid(), wxSIGTERM);
            delete m_process;
            m_process = nullptr;
        }
        
        m_initialized = false;
        m_pendingRequests.clear();
    }
    
    /**
     * Check if the server is running.
     */
    bool IsRunning() const {
        return m_process != nullptr;
    }
    
    /**
     * Check if the server is initialized and ready.
     */
    bool IsInitialized() const {
        return m_initialized;
    }
    
    /**
     * Initialize the language server.
     * Must be called after Start() before using other methods.
     */
    void Initialize(InitializeCallback callback) {
        wxString rootUri = "file://" + m_workspaceRoot;
        struct InitParams {
            int processId;
            std::string rootUri;
        };
        InitParams paramsStruct{static_cast<int>(wxGetProcessId()), std::string(rootUri.ToUTF8().data())};
        std::string params = glz::write_json(paramsStruct).value();
        int id = SendRequest("initialize", params);
        m_pendingRequests[id] = [this, callback](const std::string& resultJson) {
            // Parse the initialize response
            glz::generic result;
            auto ec = glz::read_json(result, resultJson);
            if (!ec) {
                // Send initialized notification
                SendNotification("initialized", "{}");
                m_initialized = true;
                if (callback) {
                    callback(true);
                }
            } else {
                if (callback) {
                    callback(false);
                }
            }
        };
    }
    void DidOpen(const wxString& uri, const wxString& languageId, const wxString& content) {
        struct DidOpenParams {
            struct {
                std::string uri;
                std::string languageId;
                int version = 1;
                std::string text;
            } textDocument;
        };
        DidOpenParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        paramsStruct.textDocument.languageId = std::string(languageId.ToUTF8().data());
        paramsStruct.textDocument.text = std::string(content.ToUTF8().data());
        std::string params = glz::write_json(paramsStruct).value();
        SendNotification("textDocument/didOpen", params);
    }
    
    /**
     * Notify the server that a document was saved.
     */
    void DidSave(const wxString& uri) {
        struct DidSaveParams {
            struct {
                std::string uri;
            } textDocument;
        };
        DidSaveParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        std::string params = glz::write_json(paramsStruct).value();
        SendNotification("textDocument/didSave", params);
    }
    
    /**
     * Notify the server that a document was closed.
     */
    void DidClose(const wxString& uri) {
        struct DidCloseParams {
            struct {
                std::string uri;
            } textDocument;
        };
        DidCloseParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        std::string params = glz::write_json(paramsStruct).value();
        SendNotification("textDocument/didClose", params);
    }
    
    // ========================================================================
    // Code Intelligence
    // ========================================================================
    
    /**
     * Get document symbols (outline).
     */
    void GetDocumentSymbols(const wxString& uri, SymbolsCallback callback) {
        struct TextDocumentParams {
            struct {
                std::string uri;
            } textDocument;
        };
        TextDocumentParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        std::string params = glz::write_json(paramsStruct).value();
        int id = SendRequest("textDocument/documentSymbol", params);
        m_pendingRequests[id] = [callback](const std::string& resultJson) {
            std::vector<GzDocumentSymbol> parsed;
            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, resultJson);
            std::vector<LspDocumentSymbol> symbols;
            if (!ec) {
                for (const auto& g : parsed) symbols.push_back(fromGzDocumentSymbol(g));
            }
            if (callback) {
                callback(symbols);
            }
        };
    }
    
    /**
     * Go to definition.
     */
    void GoToDefinition(const wxString& uri, const LspPosition& pos, LocationCallback callback) {
        struct DefinitionParams {
            struct {
                std::string uri;
            } textDocument;
            LspPosition position;
        };
        DefinitionParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        paramsStruct.position = pos;
        std::string params = glz::write_json(paramsStruct).value();
        int id = SendRequest("textDocument/definition", params);
        m_pendingRequests[id] = [callback](const std::string& resultJson) {
            std::vector<GzLocation> parsed;
            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, resultJson);
            std::vector<LspLocation> locations;
            if (!ec) {
                for (const auto& g : parsed) locations.push_back(fromGzLocation(g));
            }
            if (callback) {
                callback(locations);
            }
        };
    }
    
    /**
     * Find all references.
     */
    void FindReferences(const wxString& uri, const LspPosition& pos, LocationCallback callback) {
        struct ReferencesParams {
            struct {
                std::string uri;
            } textDocument;
            LspPosition position;
            struct {
                bool includeDeclaration = true;
            } context;
        };
        ReferencesParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        paramsStruct.position = pos;
        paramsStruct.context.includeDeclaration = true;
        std::string params = glz::write_json(paramsStruct).value();
        int id = SendRequest("textDocument/references", params);
        m_pendingRequests[id] = [callback](const std::string& resultJson) {
            std::vector<GzLocation> parsed;
            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, resultJson);
            std::vector<LspLocation> locations;
            if (!ec) {
                for (const auto& g : parsed) locations.push_back(fromGzLocation(g));
            }
            if (callback) {
                callback(locations);
            }
        };
    }
    
    /**
     * Get completions at position.
     */
    void GetCompletions(const wxString& uri, const LspPosition& pos, CompletionCallback callback) {
        struct CompletionParams {
            struct {
                std::string uri;
            } textDocument;
            LspPosition position;
        };
        CompletionParams paramsStruct;
        paramsStruct.textDocument.uri = std::string(uri.ToUTF8().data());
        paramsStruct.position = pos;
        std::string params = glz::write_json(paramsStruct).value();
        int id = SendRequest("textDocument/completion", params);
        m_pendingRequests[id] = [callback](const std::string& resultJson) {
            std::vector<GzCompletionItem> parsed;
            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, resultJson);
            std::vector<LspCompletionItem> items;
            if (!ec) {
                for (const auto& g : parsed) items.push_back(fromGzCompletionItem(g));
            }
            if (callback) {
                callback(items);
            }
        };
    }
    
    // ========================================================================
    // Callbacks
    // ========================================================================
    
    /**
     * Set callback for diagnostics notifications.
     */
    void SetDiagnosticsCallback(DiagnosticsCallback callback) {
        m_diagnosticsCallback = callback;
    }
    
private:
    wxString m_inputBuffer;
    
    std::map<int, std::function<void(const std::string&)>> m_pendingRequests;
    std::map<wxString, int> m_documentVersions;
    DiagnosticsCallback m_diagnosticsCallback;
    
    /**
     * Send a JSON-RPC request (expects a response).
     */
    int SendRequest(const std::string& method, const std::string& params) {
        int id = m_nextId++;
        struct Request {
            std::string jsonrpc = "2.0";
            int id;
            std::string method;
            std::string params;
        };
        Request req{.jsonrpc = "2.0", .id = id, .method = method, .params = params};
        std::string content = glz::write_json(req).value();
        SendMessage(wxString::FromUTF8(content.c_str()));
        return id;
    }
    
    /**
     * Send a JSON-RPC notification (no response expected).
     */
    void SendNotification(const std::string& method, const std::string& params) {
        struct Notification {
            std::string jsonrpc = "2.0";
            std::string method;
            std::string params;
        };
        Notification notif{.jsonrpc = "2.0", .method = method, .params = params};
        std::string content = glz::write_json(notif).value();
        SendMessage(wxString::FromUTF8(content.c_str()));
    }
    
    /**
     * Send a raw LSP message with Content-Length header.
     */
    void SendMessage(const wxString& content) {
        if (!m_process) return;
        
        wxOutputStream* out = m_process->GetOutputStream();
        if (!out) return;
        
        std::string utf8 = content.ToUTF8().data();
        std::string message = "Content-Length: " + std::to_string(utf8.size()) + "\r\n\r\n" + utf8;
        
        out->Write(message.c_str(), message.size());
    }
    
    /**
     * Poll for output from the server.
     */
    void OnPollTimer(wxTimerEvent& event) {
        if (!m_process) return;
        
        wxInputStream* in = m_process->GetInputStream();
        if (!in || !in->CanRead()) return;
        
        // Read available data
        char buffer[4096];
        while (in->CanRead()) {
            in->Read(buffer, sizeof(buffer) - 1);
            size_t bytesRead = in->LastRead();
            if (bytesRead == 0) break;
            
            buffer[bytesRead] = '\0';
            m_inputBuffer += wxString::FromUTF8(buffer);
        }
        
        // Process complete messages
        ProcessInputBuffer();
    }
    
    /**
     * Process buffered input for complete LSP messages.
     */
    void ProcessInputBuffer() {
        while (true) {
            // Look for Content-Length header
            int headerEnd = m_inputBuffer.Find("\r\n\r\n");
            if (headerEnd == wxNOT_FOUND) break;
            
            // Parse Content-Length
            wxString header = m_inputBuffer.Left(headerEnd);
            long contentLength = 0;
            
            int clPos = header.Find("Content-Length:");
            if (clPos != wxNOT_FOUND) {
                wxString lenStr = header.Mid(clPos + 15).BeforeFirst('\r').Trim(true).Trim(false);
                lenStr.ToLong(&contentLength);
            }
            
            // Check if we have the full message
            size_t messageStart = headerEnd + 4;
            if (m_inputBuffer.length() < messageStart + contentLength) break;
            
            // Extract and process the message
            wxString content = m_inputBuffer.Mid(messageStart, contentLength);
            m_inputBuffer = m_inputBuffer.Mid(messageStart + contentLength);
            
            HandleMessage(content);
        }
    }
    
    /**
     * Handle a received LSP message.
     */
    void HandleMessage(const wxString& content) {
        // Parse the message using Glaze
        std::string contentStr = std::string(content.ToUTF8().data());
        glz::generic msg;
        auto ec = glz::read_json(msg, contentStr);
        if (ec) return;

        if (msg["id"].is_number()) {
            int id = msg["id"].as<int>();
            auto it = m_pendingRequests.find(id);
            if (it != m_pendingRequests.end()) {
                std::string result_json = glz::write_json(msg["result"]).value();
                it->second(result_json);
                m_pendingRequests.erase(it);
            }
        } else if (msg["method"].is_string()) {
            std::string method = msg["method"].get<std::string>();
            if (method == "textDocument/publishDiagnostics") {
                std::string params_json = glz::write_json(msg["params"]).value();
                HandleDiagnostics(params_json);
            }
            // Handle other notifications as needed
        }
    }
    
    /**
     * Handle diagnostics notification.
     */
    void HandleDiagnostics(const std::string& params) {
        if (!m_diagnosticsCallback) return;
        DiagnosticsParams dp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(dp, params);
        if (ec) return;
        std::vector<LspDiagnostic> diagnostics;
        for (const auto& g : dp.diagnostics) {
            diagnostics.push_back(fromGzDiagnostic(g));
        }
        m_diagnosticsCallback(dp.uri, diagnostics);
    }
    
    /**
     * Handle process termination.
     */
    void OnProcessTerminated(wxProcessEvent& event) {
        m_pollTimer.Stop();
        m_process = nullptr;
        m_initialized = false;
    }
    
    /**
     * Parse document symbols from JSON response.
     */
    static void ParseSymbols(const std::string& arr, std::vector<LspDocumentSymbol>& symbols) {
        std::vector<GzDocumentSymbol> parsed;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, arr);
        if (!ec) {
            symbols.clear();
            for (const auto& g : parsed) symbols.push_back(fromGzDocumentSymbol(g));
        }
    }
    
    /**
     * Parse locations from JSON response.
     */
    static void ParseLocations(const std::string& result, std::vector<LspLocation>& locations) {
        std::vector<GzLocation> parsed;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, result);
        if (!ec) {
            locations.clear();
            for (const auto& g : parsed) locations.push_back(fromGzLocation(g));
        }
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get the icon character for a symbol kind.
 */
inline wxString GetSymbolKindIcon(LspSymbolKind kind) {
    switch (kind) {
        case LspSymbolKind::File: return "📄";
        case LspSymbolKind::Module: return "📦";
        case LspSymbolKind::Namespace: return "🏷";
        case LspSymbolKind::Package: return "📦";
        case LspSymbolKind::Class: return "🔷";
        case LspSymbolKind::Method: return "🔹";
        case LspSymbolKind::Property: return "🔸";
        case LspSymbolKind::Field: return "🔸";
        case LspSymbolKind::Constructor: return "🔧";
        case LspSymbolKind::Enum: return "📋";
        case LspSymbolKind::Interface: return "🔶";
        case LspSymbolKind::Function: return "⚡";
        case LspSymbolKind::Variable: return "📌";
        case LspSymbolKind::Constant: return "🔒";
        case LspSymbolKind::String: return "📝";
        case LspSymbolKind::Number: return "🔢";
        case LspSymbolKind::Boolean: return "✓";
        case LspSymbolKind::Array: return "📚";
        case LspSymbolKind::Object: return "📦";
        case LspSymbolKind::Struct: return "🧱";
        case LspSymbolKind::EnumMember: return "📋";
        case LspSymbolKind::Event: return "⚡";
        case LspSymbolKind::Operator: return "➕";
        case LspSymbolKind::TypeParameter: return "🅃";
        default: return "•";
    }
}

/**
 * Get a short name for a symbol kind.
 */
inline wxString GetSymbolKindName(LspSymbolKind kind) {
    switch (kind) {
        case LspSymbolKind::File: return "file";
        case LspSymbolKind::Module: return "module";
        case LspSymbolKind::Namespace: return "namespace";
        case LspSymbolKind::Package: return "package";
        case LspSymbolKind::Class: return "class";
        case LspSymbolKind::Method: return "method";
        case LspSymbolKind::Property: return "property";
        case LspSymbolKind::Field: return "field";
        case LspSymbolKind::Constructor: return "constructor";
        case LspSymbolKind::Enum: return "enum";
        case LspSymbolKind::Interface: return "interface";
        case LspSymbolKind::Function: return "function";
        case LspSymbolKind::Variable: return "variable";
        case LspSymbolKind::Constant: return "constant";
        case LspSymbolKind::String: return "string";
        case LspSymbolKind::Number: return "number";
        case LspSymbolKind::Boolean: return "boolean";
        case LspSymbolKind::Array: return "array";
        case LspSymbolKind::Object: return "object";
        case LspSymbolKind::Struct: return "struct";
        case LspSymbolKind::EnumMember: return "enum member";
        case LspSymbolKind::Event: return "event";
        case LspSymbolKind::Operator: return "operator";
        case LspSymbolKind::TypeParameter: return "type param";
        default: return "symbol";
    }
}

/**
 * Convert a file path to a file:// URI.
 */
inline wxString PathToUri(const wxString& path) {
    wxString uri = path;
    uri.Replace("\\", "/");
    if (!uri.StartsWith("/")) {
        uri = "/" + uri;
    }
    return "file://" + uri;
}

/**
 * Convert a file:// URI to a file path.
 */
inline wxString UriToPath(const wxString& uri) {
    wxString path = uri;
    if (path.StartsWith("file://")) {
        path = path.Mid(7);
    }
#ifdef __WXMSW__
    // On Windows, remove leading slash from /C:/path
    if (path.length() > 2 && path[0] == '/' && path[2] == ':') {
        path = path.Mid(1);
    }
#endif
    return path;
}

static void ParseLocations(const std::string& resultJson, std::vector<LspLocation>& locations) {
    std::vector<GzLocation> parsed;
    auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed, resultJson);
    locations.clear();
    if (!ec) {
        for (const auto& g : parsed) locations.push_back(fromGzLocation(g));
    }
}

// Glaze meta specializations for LSP types
namespace glz {

} // namespace glz

#endif // LSP_CLIENT_H
