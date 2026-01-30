#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H

#include <functional>
#include <map>
#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <glaze/glaze.hpp>

// Platform-specific includes for process management
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#endif

/**
 * LSP (Language Server Protocol) client for ByteMuseHQ.
 * 
 * Platform-independent implementation that communicates with language servers
 * via JSON-RPC over stdin/stdout. Provides code intelligence features like
 * go-to-definition, find references, document symbols, and diagnostics.
 */

// ============================================================================
// LSP Data Types
// ============================================================================

struct LspPosition {
    int line = 0;
    int character = 0;
};

struct LspRange {
    LspPosition start;
    LspPosition end;
};

struct LspLocation {
    std::string uri;
    LspRange range;
};

enum class LspSymbolKind {
    File = 1, Module = 2, Namespace = 3, Package = 4,
    Class = 5, Method = 6, Property = 7, Field = 8,
    Constructor = 9, Enum = 10, Interface = 11, Function = 12,
    Variable = 13, Constant = 14, String = 15, Number = 16,
    Boolean = 17, Array = 18, Object = 19, Key = 20,
    Null = 21, EnumMember = 22, Struct = 23, Event = 24,
    Operator = 25, TypeParameter = 26
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

// Glaze metadata for serialization/deserialization
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
    static constexpr auto value = object(
        "name", &T::name,
        "detail", &T::detail,
        "kind", &T::kind,
        "range", &T::range,
        "selectionRange", &T::selectionRange,
        "children", &T::children
    );
};

template<> struct glz::meta<LspDiagnostic> {
    using T = LspDiagnostic;
    static constexpr auto value = object(
        "range", &T::range,
        "severity", &T::severity,
        "code", &T::code,
        "source", &T::source,
        "message", &T::message
    );
};

template<> struct glz::meta<LspCompletionItem> {
    using T = LspCompletionItem;
    static constexpr auto value = object(
        "label", &T::label,
        "kind", &T::kind,
        "detail", &T::detail,
        "documentation", &T::documentation,
        "insertText", &T::insertText
    );
};

// ============================================================================
// Callbacks
// ============================================================================

using InitializeCallback = std::function<void(bool success)>;
using SymbolsCallback = std::function<void(const std::vector<LspDocumentSymbol>& symbols)>;
using LocationCallback = std::function<void(const std::vector<LspLocation>& locations)>;
using DiagnosticsCallback = std::function<void(const std::string& uri, const std::vector<LspDiagnostic>& diagnostics)>;
using CompletionCallback = std::function<void(const std::vector<LspCompletionItem>& items)>;
using LogCallback = std::function<void(const std::string& message)>;

// ============================================================================
// SSH Configuration
// ============================================================================

struct LspSshConfig {
    bool enabled = false;
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        
        std::string cmd = "ssh -tt";
        
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
// Process Handle (Platform-independent wrapper)
// ============================================================================

class ProcessHandle {
public:
    virtual ~ProcessHandle() = default;
    virtual bool isRunning() const = 0;
    virtual void terminate() = 0;
    virtual int readStdout(char* buffer, size_t size) = 0;
    virtual int readStderr(char* buffer, size_t size) = 0;
    virtual bool writeStdin(const char* data, size_t size) = 0;
};

#ifndef _WIN32
class UnixProcessHandle : public ProcessHandle {
private:
    pid_t m_pid;
    int m_stdin_fd;
    int m_stdout_fd;
    int m_stderr_fd;
    
public:
    UnixProcessHandle(pid_t pid, int stdin_fd, int stdout_fd, int stderr_fd)
        : m_pid(pid), m_stdin_fd(stdin_fd), m_stdout_fd(stdout_fd), m_stderr_fd(stderr_fd) {}
    
    ~UnixProcessHandle() override {
        terminate();
        if (m_stdin_fd >= 0) close(m_stdin_fd);
        if (m_stdout_fd >= 0) close(m_stdout_fd);
        if (m_stderr_fd >= 0) close(m_stderr_fd);
    }
    
    bool isRunning() const override {
        int status;
        pid_t result = waitpid(m_pid, &status, WNOHANG);
        return result == 0;
    }
    
    void terminate() override {
        if (m_pid > 0) {
            kill(m_pid, SIGTERM);
            m_pid = -1;
        }
    }
    
    int readStdout(char* buffer, size_t size) override {
        if (m_stdout_fd < 0) return -1;
        
        // Non-blocking read
        pollfd pfd = {m_stdout_fd, POLLIN, 0};
        int ret = poll(&pfd, 1, 0);
        if (ret <= 0) return 0;
        
        return read(m_stdout_fd, buffer, size);
    }
    
    int readStderr(char* buffer, size_t size) override {
        if (m_stderr_fd < 0) return -1;
        
        pollfd pfd = {m_stderr_fd, POLLIN, 0};
        int ret = poll(&pfd, 1, 0);
        if (ret <= 0) return 0;
        
        return read(m_stderr_fd, buffer, size);
    }
    
    bool writeStdin(const char* data, size_t size) override {
        if (m_stdin_fd < 0) return false;
        ssize_t written = write(m_stdin_fd, data, size);
        return written == static_cast<ssize_t>(size);
    }
};
#endif

// ============================================================================
// LSP Client
// ============================================================================

class LspClient {
private:
    std::unique_ptr<ProcessHandle> m_process;
    std::string m_workspaceRoot;
    int m_nextId = 1;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    LspSshConfig m_sshConfig;
    
    std::thread m_readerThread;
    std::mutex m_mutex;
    std::string m_inputBuffer;
    
    std::map<int, std::function<void(const glz::generic&)>> m_pendingRequests;
    DiagnosticsCallback m_diagnosticsCallback;
    LogCallback m_logCallback;
    
public:
    LspClient() = default;
    
    ~LspClient() {
        stop();
    }
    
    void setLogCallback(LogCallback callback) {
        m_logCallback = callback;
    }
    
    void setSshConfig(const LspSshConfig& config) {
        m_sshConfig = config;
    }
    
    LspSshConfig getSshConfig() const {
        return m_sshConfig;
    }
    
    bool isRemoteExecution() const {
        return m_sshConfig.isValid();
    }
    
    bool start(const std::string& command, const std::string& workspaceRoot) {
        if (m_process) {
            stop();
        }
        
        m_workspaceRoot = workspaceRoot;
        
        std::string fullCommand = command + " --background-index";
        
        if (m_sshConfig.isValid()) {
            std::string sshPrefix = m_sshConfig.buildSshPrefix();
            fullCommand = sshPrefix + " \"cd '" + workspaceRoot + "' && " + fullCommand + "\"";
        }
        
        log("Starting LSP server: " + fullCommand);
        
#ifndef _WIN32
        int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
        
        if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
            log("Failed to create pipes");
            return false;
        }
        
        pid_t pid = fork();
        if (pid < 0) {
            log("Failed to fork process");
            return false;
        }
        
        if (pid == 0) {
            // Child process
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);
            
            close(stdin_pipe[0]);
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);
            
            execl("/bin/sh", "sh", "-c", fullCommand.c_str(), nullptr);
            _exit(1);
        }
        
        // Parent process
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        m_process = std::make_unique<UnixProcessHandle>(
            pid, stdin_pipe[1], stdout_pipe[0], stderr_pipe[0]
        );
#else
        // Windows implementation would go here
        log("Windows not yet supported");
        return false;
#endif
        
        m_running = true;
        m_readerThread = std::thread(&LspClient::readerThreadFunc, this);
        
        return true;
    }
    
    void stop() {
        if (!m_process) return;
        
        log("Stopping LSP server");
        
        if (m_initialized) {
            sendRequest("shutdown", glz::generic{});
            sendNotification("exit", glz::generic{});
        }
        
        m_running = false;
        
        if (m_readerThread.joinable()) {
            m_readerThread.join();
        }
        
        m_process.reset();
        m_initialized = false;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests.clear();
    }
    
    bool isRunning() const {
        return m_process && m_process->isRunning();
    }
    
    bool isInitialized() const {
        return m_initialized;
    }
    
    void initialize(InitializeCallback callback) {
        glz::generic params;
        params["processId"] = getpid();
        params["rootUri"] = "file://" + m_workspaceRoot;
        params["capabilities"] = glz::generic{};
        
        int id = sendRequest("initialize", params);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = [this, callback](const glz::generic& result) {
            log("Server initialized");
            sendNotification("initialized", glz::generic{});
            m_initialized = true;
            if (callback) callback(true);
        };
    }
    
    void didOpen(const std::string& uri, const std::string& languageId, const std::string& content) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        params["textDocument"]["languageId"] = languageId;
        params["textDocument"]["version"] = 1;
        params["textDocument"]["text"] = content;
        
        sendNotification("textDocument/didOpen", params);
    }
    
    void didChange(const std::string& uri, int version, const std::string& content) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        params["textDocument"]["version"] = version;
        params["contentChanges"] = std::vector<glz::generic>();
        params["contentChanges"][0] = glz::generic{};
        params["contentChanges"][0]["text"] = content;
        
        sendNotification("textDocument/didChange", params);
    }
    
    void didSave(const std::string& uri) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        
        sendNotification("textDocument/didSave", params);
    }
    
    void didClose(const std::string& uri) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        
        sendNotification("textDocument/didClose", params);
    }
    
    void getDocumentSymbols(const std::string& uri, SymbolsCallback callback) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        
        int id = sendRequest("textDocument/documentSymbol", params);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = [callback](const glz::generic& result) {
            std::vector<LspDocumentSymbol> symbols;
            if (result.is_array()) {
                std::string json = glz::write_json(result).value_or("[]");
                [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(symbols, json);
            }
            if (callback) callback(symbols);
        };
    }
    
    void goToDefinition(const std::string& uri, const LspPosition& pos, LocationCallback callback) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        params["position"] = glz::generic{};
        params["position"]["line"] = pos.line;
        params["position"]["character"] = pos.character;
        
        int id = sendRequest("textDocument/definition", params);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = [callback](const glz::generic& result) {
            std::vector<LspLocation> locations;
            if (result.is_array()) {
                std::string json = glz::write_json(result).value_or("[]");
                [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(locations, json);
            }
            if (callback) callback(locations);
        };
    }
    
    void findReferences(const std::string& uri, const LspPosition& pos, LocationCallback callback) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        params["position"] = glz::generic{};
        params["position"]["line"] = pos.line;
        params["position"]["character"] = pos.character;
        params["context"] = glz::generic{};
        params["context"]["includeDeclaration"] = true;
        
        int id = sendRequest("textDocument/references", params);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = [callback](const glz::generic& result) {
            std::vector<LspLocation> locations;
            if (result.is_array()) {
                std::string json = glz::write_json(result).value_or("[]");
                [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(locations, json);
            }
            if (callback) callback(locations);
        };
    }
    
    void getCompletions(const std::string& uri, const LspPosition& pos, CompletionCallback callback) {
        glz::generic params;
        params["textDocument"] = glz::generic{};
        params["textDocument"]["uri"] = uri;
        params["position"] = glz::generic{};
        params["position"]["line"] = pos.line;
        params["position"]["character"] = pos.character;
        
        int id = sendRequest("textDocument/completion", params);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = [callback](const glz::generic& result) {
            std::vector<LspCompletionItem> items;
            if (result.is_object() && result.contains("items")) {
                std::string json = glz::write_json(result["items"]).value_or("[]");
                [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(items, json);
            } else if (result.is_array()) {
                std::string json = glz::write_json(result).value_or("[]");
                [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(items, json);
            }
            if (callback) callback(items);
        };
    }
    
    void setDiagnosticsCallback(DiagnosticsCallback callback) {
        m_diagnosticsCallback = callback;
    }
    
private:
    void log(const std::string& message) {
        if (m_logCallback) {
            m_logCallback(message);
        }
    }
    
    int sendRequest(const std::string& method, const glz::generic& params) {
        int id = m_nextId++;
        
        glz::generic msg;
        msg["jsonrpc"] = "2.0";
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
        
        sendMessage(msg);
        return id;
    }
    
    void sendNotification(const std::string& method, const glz::generic& params) {
        glz::generic msg;
        msg["jsonrpc"] = "2.0";
        msg["method"] = method;
        msg["params"] = params;
        
        sendMessage(msg);
    }
    
    void sendMessage(const glz::generic& msg) {
        if (!m_process) return;
        
        std::string content = glz::write_json(msg).value_or("{}");
        std::string message = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
        
        if (!m_process->writeStdin(message.c_str(), message.size())) {
            log("Failed to write to LSP server stdin");
        }
    }
    
    void readerThreadFunc() {
        char buffer[4096];
        
        while (m_running && m_process) {
            int bytesRead = m_process->readStdout(buffer, sizeof(buffer) - 1);
            
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                
                std::lock_guard<std::mutex> lock(m_mutex);
                m_inputBuffer += buffer;
                processInputBuffer();
            } else if (bytesRead < 0) {
                log("Error reading from LSP server");
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void processInputBuffer() {
        while (true) {
            size_t header_end = m_inputBuffer.find("\r\n\r\n");
            if (header_end == std::string::npos) break;
            
            size_t content_length = 0;
            size_t cl_pos = m_inputBuffer.find("Content-Length:");
            if (cl_pos != std::string::npos && cl_pos < header_end) {
                size_t num_start = cl_pos + 15;
                while (num_start < header_end && isspace(m_inputBuffer[num_start])) num_start++;
                content_length = std::stoul(m_inputBuffer.substr(num_start));
            }
            
            size_t message_start = header_end + 4;
            if (m_inputBuffer.size() < message_start + content_length) break;
            
            std::string content = m_inputBuffer.substr(message_start, content_length);
            m_inputBuffer = m_inputBuffer.substr(message_start + content_length);
            
            handleMessage(content);
        }
    }
    
    void handleMessage(const std::string& content) {
        glz::generic msg;
        auto ec = glz::read_json(msg, content);
        if (ec) {
            log("Failed to parse LSP message: " + content);
            return;
        }
        
        if (msg.contains("id") && !msg["id"].is_null()) {
            int id = static_cast<int>(msg["id"].get<double>());
            auto it = m_pendingRequests.find(id);
            if (it != m_pendingRequests.end()) {
                if (msg.contains("result")) {
                    it->second(msg["result"]);
                } else if (msg.contains("error")) {
                    log("LSP error: " + glz::write_json(msg["error"]).value_or("unknown"));
                }
                m_pendingRequests.erase(it);
            }
        } else if (msg.contains("method")) {
            std::string method = msg["method"].get<std::string>();
            if (method == "textDocument/publishDiagnostics" && m_diagnosticsCallback) {
                handleDiagnostics(msg["params"]);
            }
        }
    }
    
    void handleDiagnostics(const glz::generic& params) {
        if (!params.is_object()) return;
        
        std::string uri = params["uri"].get<std::string>();
        std::vector<LspDiagnostic> diagnostics;
        
        if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
            std::string json = glz::write_json(params["diagnostics"]).value_or("[]");
            [[maybe_unused]] auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(diagnostics, json);
        }
        
        if (m_diagnosticsCallback) {
            m_diagnosticsCallback(uri, diagnostics);
        }
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

inline std::string getSymbolKindIcon(LspSymbolKind kind) {
    switch (kind) {
        case LspSymbolKind::Function: return "⚡";
        case LspSymbolKind::Class: return "🔷";
        case LspSymbolKind::Method: return "🔹";
        case LspSymbolKind::Variable: return "📌";
        case LspSymbolKind::Struct: return "🧱";
        case LspSymbolKind::Namespace: return "🏷";
        default: return "•";
    }
}

inline std::string pathToUri(const std::string& path) {
    std::string uri = path;
    if (uri[0] != '/') uri = "/" + uri;
    return "file://" + uri;
}

inline std::string uriToPath(const std::string& uri) {
    if (uri.find("file://") == 0) {
        return uri.substr(7);
    }
    return uri;
}

#endif // LSP_CLIENT_H
