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
    
    wxString ToJson() const {
        return wxString::Format(R"({"line":%d,"character":%d})", line, character);
    }
};

struct LspRange {
    LspPosition start;
    LspPosition end;
    
    wxString ToJson() const {
        return wxString::Format(R"({"start":%s,"end":%s})", 
            start.ToJson(), end.ToJson());
    }
};

struct LspLocation {
    wxString uri;
    LspRange range;
};

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
    wxString name;
    wxString detail;
    LspSymbolKind kind;
    LspRange range;
    LspRange selectionRange;
    std::vector<LspDocumentSymbol> children;
};

struct LspDiagnostic {
    LspRange range;
    int severity; // 1=Error, 2=Warning, 3=Info, 4=Hint
    wxString code;
    wxString source;
    wxString message;
};

struct LspCompletionItem {
    wxString label;
    int kind;
    wxString detail;
    wxString documentation;
    wxString insertText;
};

// ============================================================================
// Callbacks
// ============================================================================

using InitializeCallback = std::function<void(bool success)>;
using SymbolsCallback = std::function<void(const std::vector<LspDocumentSymbol>& symbols)>;
using LocationCallback = std::function<void(const std::vector<LspLocation>& locations)>;
using DiagnosticsCallback = std::function<void(const wxString& uri, const std::vector<LspDiagnostic>& diagnostics)>;
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
class JsonBuilder {
public:
    static wxString Object(const std::vector<std::pair<wxString, wxString>>& fields) {
        wxString result = "{";
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) result += ",";
            result += "\"" + fields[i].first + "\":" + fields[i].second;
        }
        result += "}";
        return result;
    }
    
    static wxString String(const wxString& s) {
        wxString escaped = s;
        escaped.Replace("\\", "\\\\");
        escaped.Replace("\"", "\\\"");
        escaped.Replace("\n", "\\n");
        escaped.Replace("\r", "\\r");
        escaped.Replace("\t", "\\t");
        return "\"" + escaped + "\"";
    }
    
    static wxString Number(int n) {
        return wxString::Format("%d", n);
    }
    
    static wxString Bool(bool b) {
        return b ? "true" : "false";
    }
    
    static wxString Null() {
        return "null";
    }
    
    static wxString Array(const std::vector<wxString>& items) {
        wxString result = "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) result += ",";
            result += items[i];
        }
        result += "]";
        return result;
    }
};

// ============================================================================
// Simple JSON Parser (minimal implementation)
// ============================================================================

/**
 * Minimal JSON parser for LSP responses.
 * Handles basic types: objects, arrays, strings, numbers, bools, null.
 */
class JsonValue {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    
    Type type = Null;
    bool boolValue = false;
    double numberValue = 0;
    wxString stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<wxString, JsonValue> objectValue;
    
    bool IsNull() const { return type == Null; }
    bool IsBool() const { return type == Bool; }
    bool IsNumber() const { return type == Number; }
    bool IsString() const { return type == String; }
    bool IsArray() const { return type == Array; }
    bool IsObject() const { return type == Object; }
    
    const JsonValue& operator[](const wxString& key) const {
        static JsonValue null;
        if (type != Object) return null;
        auto it = objectValue.find(key);
        return it != objectValue.end() ? it->second : null;
    }
    
    const JsonValue& operator[](size_t index) const {
        static JsonValue null;
        if (type != Array || index >= arrayValue.size()) return null;
        return arrayValue[index];
    }
    
    int GetInt(int defaultVal = 0) const {
        return type == Number ? static_cast<int>(numberValue) : defaultVal;
    }
    
    wxString GetString(const wxString& defaultVal = "") const {
        return type == String ? stringValue : defaultVal;
    }
    
    bool GetBool(bool defaultVal = false) const {
        return type == Bool ? boolValue : defaultVal;
    }
    
    size_t Size() const {
        return type == Array ? arrayValue.size() : 0;
    }
    
    bool Has(const wxString& key) const {
        return type == Object && objectValue.find(key) != objectValue.end();
    }
    
    static JsonValue Parse(const wxString& json);
    
private:
    static JsonValue ParseValue(const wxString& json, size_t& pos);
    static void SkipWhitespace(const wxString& json, size_t& pos);
    static wxString ParseString(const wxString& json, size_t& pos);
    static double ParseNumber(const wxString& json, size_t& pos);
};

// JSON Parser implementation
inline void JsonValue::SkipWhitespace(const wxString& json, size_t& pos) {
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || 
           json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }
}

inline wxString JsonValue::ParseString(const wxString& json, size_t& pos) {
    pos++; // Skip opening quote
    wxString result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++;
            switch (json[pos].IsAscii() ? static_cast<char>(json[pos]) : 0) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    pos++; // Skip closing quote
    return result;
}

inline double JsonValue::ParseNumber(const wxString& json, size_t& pos) {
    size_t start = pos;
    if (json[pos] == '-') pos++;
    while (pos < json.length() && (json[pos] >= '0' && json[pos] <= '9')) pos++;
    if (pos < json.length() && json[pos] == '.') {
        pos++;
        while (pos < json.length() && (json[pos] >= '0' && json[pos] <= '9')) pos++;
    }
    if (pos < json.length() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (pos < json.length() && (json[pos] == '+' || json[pos] == '-')) pos++;
        while (pos < json.length() && (json[pos] >= '0' && json[pos] <= '9')) pos++;
    }
    double val = 0;
    json.Mid(start, pos - start).ToDouble(&val);
    return val;
}

inline JsonValue JsonValue::ParseValue(const wxString& json, size_t& pos) {
    SkipWhitespace(json, pos);
    if (pos >= json.length()) return JsonValue();
    
    JsonValue value;
    char c = json[pos].IsAscii() ? static_cast<char>(json[pos]) : 0;
    
    if (c == '"') {
        value.type = String;
        value.stringValue = ParseString(json, pos);
    } else if (c == '{') {
        value.type = Object;
        pos++; // Skip '{'
        SkipWhitespace(json, pos);
        while (pos < json.length() && json[pos] != '}') {
            SkipWhitespace(json, pos);
            if (json[pos] == '"') {
                wxString key = ParseString(json, pos);
                SkipWhitespace(json, pos);
                if (pos < json.length() && json[pos] == ':') pos++;
                value.objectValue[key] = ParseValue(json, pos);
                SkipWhitespace(json, pos);
                if (pos < json.length() && json[pos] == ',') pos++;
            } else {
                break;
            }
        }
        if (pos < json.length()) pos++; // Skip '}'
    } else if (c == '[') {
        value.type = Array;
        pos++; // Skip '['
        SkipWhitespace(json, pos);
        while (pos < json.length() && json[pos] != ']') {
            value.arrayValue.push_back(ParseValue(json, pos));
            SkipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') pos++;
            SkipWhitespace(json, pos);
        }
        if (pos < json.length()) pos++; // Skip ']'
    } else if (c == 't' && json.Mid(pos, 4) == "true") {
        value.type = Bool;
        value.boolValue = true;
        pos += 4;
    } else if (c == 'f' && json.Mid(pos, 5) == "false") {
        value.type = Bool;
        value.boolValue = false;
        pos += 5;
    } else if (c == 'n' && json.Mid(pos, 4) == "null") {
        value.type = Null;
        pos += 4;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        value.type = Number;
        value.numberValue = ParseNumber(json, pos);
    }
    
    return value;
}

inline JsonValue JsonValue::Parse(const wxString& json) {
    size_t pos = 0;
    return ParseValue(json, pos);
}

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
public:
    LspClient() : m_process(nullptr), m_nextId(1), m_initialized(false) {}
    
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
        m_pollTimer.Bind(wxEVT_TIMER, &LspClient::OnPollTimer, this);
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
        
        wxString params = JsonBuilder::Object({
            {"processId", JsonBuilder::Number(wxGetProcessId())},
            {"rootUri", JsonBuilder::String(rootUri)},
            {"capabilities", JsonBuilder::Object({
                {"textDocument", JsonBuilder::Object({
                    {"synchronization", JsonBuilder::Object({
                        {"didSave", JsonBuilder::Bool(true)}
                    })},
                    {"completion", JsonBuilder::Object({
                        {"completionItem", JsonBuilder::Object({
                            {"snippetSupport", JsonBuilder::Bool(false)}
                        })}
                    })},
                    {"documentSymbol", JsonBuilder::Object({
                        {"hierarchicalDocumentSymbolSupport", JsonBuilder::Bool(true)}
                    })},
                    {"definition", JsonBuilder::Object({})},
                    {"references", JsonBuilder::Object({})},
                    {"hover", JsonBuilder::Object({})}
                })}
            })}
        });
        
        int id = SendRequest("initialize", params);
        m_pendingRequests[id] = [this, callback](const JsonValue& result) {
            m_initialized = !result.IsNull();
            
            // Send initialized notification
            if (m_initialized) {
                SendNotification("initialized", "{}");
            }
            
            if (callback) {
                callback(m_initialized);
            }
        };
    }
    
    // ========================================================================
    // Document Management
    // ========================================================================
    
    /**
     * Notify the server that a document was opened.
     */
    void DidOpen(const wxString& uri, const wxString& languageId, const wxString& content) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)},
                {"languageId", JsonBuilder::String(languageId)},
                {"version", JsonBuilder::Number(1)},
                {"text", JsonBuilder::String(content)}
            })}
        });
        
        SendNotification("textDocument/didOpen", params);
        m_documentVersions[uri] = 1;
    }
    
    /**
     * Notify the server that a document was changed.
     */
    void DidChange(const wxString& uri, const wxString& content) {
        int version = ++m_documentVersions[uri];
        
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)},
                {"version", JsonBuilder::Number(version)}
            })},
            {"contentChanges", JsonBuilder::Array({
                JsonBuilder::Object({
                    {"text", JsonBuilder::String(content)}
                })
            })}
        });
        
        SendNotification("textDocument/didChange", params);
    }
    
    /**
     * Notify the server that a document was closed.
     */
    void DidClose(const wxString& uri) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })}
        });
        
        SendNotification("textDocument/didClose", params);
        m_documentVersions.erase(uri);
    }
    
    /**
     * Notify the server that a document was saved.
     */
    void DidSave(const wxString& uri) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })}
        });
        
        SendNotification("textDocument/didSave", params);
    }
    
    // ========================================================================
    // Code Intelligence
    // ========================================================================
    
    /**
     * Get document symbols (outline).
     */
    void GetDocumentSymbols(const wxString& uri, SymbolsCallback callback) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })}
        });
        
        int id = SendRequest("textDocument/documentSymbol", params);
        m_pendingRequests[id] = [callback](const JsonValue& result) {
            std::vector<LspDocumentSymbol> symbols;
            if (result.IsArray()) {
                ParseSymbols(result, symbols);
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
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })},
            {"position", pos.ToJson()}
        });
        
        int id = SendRequest("textDocument/definition", params);
        m_pendingRequests[id] = [callback](const JsonValue& result) {
            std::vector<LspLocation> locations;
            ParseLocations(result, locations);
            if (callback) {
                callback(locations);
            }
        };
    }
    
    /**
     * Find all references.
     */
    void FindReferences(const wxString& uri, const LspPosition& pos, LocationCallback callback) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })},
            {"position", pos.ToJson()},
            {"context", JsonBuilder::Object({
                {"includeDeclaration", JsonBuilder::Bool(true)}
            })}
        });
        
        int id = SendRequest("textDocument/references", params);
        m_pendingRequests[id] = [callback](const JsonValue& result) {
            std::vector<LspLocation> locations;
            ParseLocations(result, locations);
            if (callback) {
                callback(locations);
            }
        };
    }
    
    /**
     * Get completions at position.
     */
    void GetCompletions(const wxString& uri, const LspPosition& pos, CompletionCallback callback) {
        wxString params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uri)}
            })},
            {"position", pos.ToJson()}
        });
        
        int id = SendRequest("textDocument/completion", params);
        m_pendingRequests[id] = [callback](const JsonValue& result) {
            std::vector<LspCompletionItem> items;
            
            const JsonValue* itemsArray = nullptr;
            if (result.IsArray()) {
                itemsArray = &result;
            } else if (result.IsObject() && result.Has("items")) {
                itemsArray = &result["items"];
            }
            
            if (itemsArray && itemsArray->IsArray()) {
                for (size_t i = 0; i < itemsArray->Size(); ++i) {
                    const auto& item = (*itemsArray)[i];
                    LspCompletionItem ci;
                    ci.label = item["label"].GetString();
                    ci.kind = item["kind"].GetInt();
                    ci.detail = item["detail"].GetString();
                    ci.insertText = item["insertText"].GetString(ci.label);
                    items.push_back(ci);
                }
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
    wxProcess* m_process;
    wxString m_workspaceRoot;
    int m_nextId;
    bool m_initialized;
    wxTimer m_pollTimer;
    wxString m_inputBuffer;
    LspSshConfig m_sshConfig;
    
    std::map<int, std::function<void(const JsonValue&)>> m_pendingRequests;
    std::map<wxString, int> m_documentVersions;
    DiagnosticsCallback m_diagnosticsCallback;
    
    /**
     * Send a JSON-RPC request (expects a response).
     */
    int SendRequest(const wxString& method, const wxString& params) {
        int id = m_nextId++;
        
        wxString content = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"id", JsonBuilder::Number(id)},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        
        SendMessage(content);
        return id;
    }
    
    /**
     * Send a JSON-RPC notification (no response expected).
     */
    void SendNotification(const wxString& method, const wxString& params) {
        wxString content = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        
        SendMessage(content);
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
        JsonValue msg = JsonValue::Parse(content);
        
        if (msg.Has("id")) {
            // Response to a request
            int id = msg["id"].GetInt();
            auto it = m_pendingRequests.find(id);
            if (it != m_pendingRequests.end()) {
                it->second(msg["result"]);
                m_pendingRequests.erase(it);
            }
        } else if (msg.Has("method")) {
            // Notification from server
            wxString method = msg["method"].GetString();
            
            if (method == "textDocument/publishDiagnostics") {
                HandleDiagnostics(msg["params"]);
            }
            // Handle other notifications as needed
        }
    }
    
    /**
     * Handle diagnostics notification.
     */
    void HandleDiagnostics(const JsonValue& params) {
        if (!m_diagnosticsCallback) return;
        
        wxString uri = params["uri"].GetString();
        std::vector<LspDiagnostic> diagnostics;
        
        const auto& diags = params["diagnostics"];
        if (diags.IsArray()) {
            for (size_t i = 0; i < diags.Size(); ++i) {
                const auto& d = diags[i];
                LspDiagnostic diag;
                diag.message = d["message"].GetString();
                diag.severity = d["severity"].GetInt(1);
                diag.source = d["source"].GetString();
                
                const auto& range = d["range"];
                diag.range.start.line = range["start"]["line"].GetInt();
                diag.range.start.character = range["start"]["character"].GetInt();
                diag.range.end.line = range["end"]["line"].GetInt();
                diag.range.end.character = range["end"]["character"].GetInt();
                
                diagnostics.push_back(diag);
            }
        }
        
        m_diagnosticsCallback(uri, diagnostics);
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
    static void ParseSymbols(const JsonValue& arr, std::vector<LspDocumentSymbol>& symbols) {
        for (size_t i = 0; i < arr.Size(); ++i) {
            const auto& item = arr[i];
            LspDocumentSymbol sym;
            sym.name = item["name"].GetString();
            sym.detail = item["detail"].GetString();
            sym.kind = static_cast<LspSymbolKind>(item["kind"].GetInt());
            
            const auto& range = item["range"];
            sym.range.start.line = range["start"]["line"].GetInt();
            sym.range.start.character = range["start"]["character"].GetInt();
            sym.range.end.line = range["end"]["line"].GetInt();
            sym.range.end.character = range["end"]["character"].GetInt();
            
            const auto& selRange = item["selectionRange"];
            sym.selectionRange.start.line = selRange["start"]["line"].GetInt();
            sym.selectionRange.start.character = selRange["start"]["character"].GetInt();
            sym.selectionRange.end.line = selRange["end"]["line"].GetInt();
            sym.selectionRange.end.character = selRange["end"]["character"].GetInt();
            
            // Parse children recursively
            if (item.Has("children") && item["children"].IsArray()) {
                ParseSymbols(item["children"], sym.children);
            }
            
            symbols.push_back(sym);
        }
    }
    
    /**
     * Parse locations from JSON response.
     */
    static void ParseLocations(const JsonValue& result, std::vector<LspLocation>& locations) {
        if (result.IsArray()) {
            for (size_t i = 0; i < result.Size(); ++i) {
                LspLocation loc;
                loc.uri = result[i]["uri"].GetString();
                const auto& range = result[i]["range"];
                loc.range.start.line = range["start"]["line"].GetInt();
                loc.range.start.character = range["start"]["character"].GetInt();
                loc.range.end.line = range["end"]["line"].GetInt();
                loc.range.end.character = range["end"]["character"].GetInt();
                locations.push_back(loc);
            }
        } else if (result.IsObject() && result.Has("uri")) {
            LspLocation loc;
            loc.uri = result["uri"].GetString();
            const auto& range = result["range"];
            loc.range.start.line = range["start"]["line"].GetInt();
            loc.range.start.character = range["start"]["character"].GetInt();
            loc.range.end.line = range["end"]["line"].GetInt();
            loc.range.end.character = range["end"]["character"].GetInt();
            locations.push_back(loc);
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

#endif // LSP_CLIENT_H
