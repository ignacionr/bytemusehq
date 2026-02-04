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
#include <condition_variable>
#include <glaze/glaze.hpp>

// Platform-specific includes for process management
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
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
    std::string remoteCommand;  // Command to run clangd on remote (e.g., "nix develop -c clangd")
    
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        
        // Use -T to disable TTY allocation - critical for binary protocols like LSP
        // TTY mode causes input echo which corrupts the LSP JSON-RPC communication
        std::string cmd = "ssh -T";
        
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
    
    /**
     * Get the command to run the LSP server on the remote.
     * If remoteCommand is set, uses that. Otherwise tries to detect nix
     * or falls back to the provided command.
     */
    std::string getRemoteLspCommand(const std::string& defaultCommand) const {
        if (!remoteCommand.empty()) {
            return remoteCommand;
        }
        return defaultCommand;
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
        
        size_t total_written = 0;
        int retries = 0;
        const int max_retries = 100; // Max ~100ms of retries
        
        while (total_written < size) {
            ssize_t written = write(m_stdin_fd, data + total_written, size - total_written);
            if (written < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Non-blocking write would block
                    retries++;
                    if (retries >= max_retries) {
                        // Give up after max retries to prevent UI freeze
                        return false;
                    }
                    usleep(1000); // 1ms
                    continue;
                }
                return false; // Real error
            }
            total_written += written;
            retries = 0; // Reset retry counter on successful write
        }
        return true;
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
    std::thread m_writerThread;
    std::thread m_stderrThread;  // Read stderr for errors and debug output
    std::mutex m_mutex;
    std::mutex m_writeMutex;
    std::string m_inputBuffer;
    std::queue<std::string> m_writeQueue;
    std::condition_variable m_writeCondition;
    
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
        log("=== LSP Client Start ===");
        log("Input command: " + command);
        log("Workspace root: " + workspaceRoot);
        log("SSH config valid: " + std::string(m_sshConfig.isValid() ? "yes" : "no"));
        
        if (m_sshConfig.isValid()) {
            log("SSH host: " + m_sshConfig.host);
            log("SSH user: " + m_sshConfig.user);
            log("SSH port: " + std::to_string(m_sshConfig.port));
            log("SSH remoteCommand: " + m_sshConfig.remoteCommand);
        }
        
        if (m_process) {
            log("Stopping existing process");
            stop();
        }
        
        m_workspaceRoot = workspaceRoot;
        
        std::string fullCommand;
        
        if (m_sshConfig.isValid()) {
            // Remote execution via SSH
            std::string sshPrefix = m_sshConfig.buildSshPrefix();
            log("SSH prefix: " + sshPrefix);
            
            std::string remoteCmd = m_sshConfig.getRemoteLspCommand(command);
            log("Remote command (after getRemoteLspCommand): " + remoteCmd);
            
            // Build the remote command with proper escaping
            // Try to detect nix environment and use appropriate wrapper
            std::string nixWrapper = "";
            if (remoteCmd.find("nix") == std::string::npos) {
                log("No 'nix' found in remoteCmd, using auto-detection wrapper");
                // User didn't specify a nix command, so we'll try to detect nix on remote
                // and wrap the command appropriately
                // Use nix-shell which doesn't require experimental features
                nixWrapper = 
                    "echo '[LSP] Starting clangd detection...' >&2; "
                    "if command -v " + remoteCmd + " >/dev/null 2>&1; then "
                    "  echo '[LSP] Found " + remoteCmd + " in PATH, using directly' >&2; "
                    "  exec " + remoteCmd + " --background-index; "
                    "elif command -v nix-shell >/dev/null 2>&1; then "
                    "  echo '[LSP] Using nix-shell -p clang-tools' >&2; "
                    "  exec nix-shell -p clang-tools --run '" + remoteCmd + " --background-index'; "
                    "elif command -v nix >/dev/null 2>&1; then "
                    "  echo '[LSP] Using nix shell with experimental features' >&2; "
                    "  exec nix --extra-experimental-features 'nix-command flakes' shell nixpkgs#clang-tools -c " + remoteCmd + " --background-index; "
                    "else "
                    "  echo '[LSP] Error: clangd not found on remote and nix not available' >&2; exit 1; "
                    "fi";
                fullCommand = sshPrefix + " \"cd '" + workspaceRoot + "' && " + nixWrapper + "\"";
                log("Using nixWrapper auto-detection");
            } else {
                log("'nix' found in remoteCmd, using user-specified command");
                // User specified a nix-aware command
                // If it's nix develop, wrap with flake.nix check and fallback to nix-shell
                std::string adjustedCmd = remoteCmd;
                
                if (remoteCmd.find("nix develop") != std::string::npos ||
                    remoteCmd.find("nix run") != std::string::npos ||
                    remoteCmd.find("nix shell") != std::string::npos) {
                    
                    // Add experimental features flag if needed
                    if (remoteCmd.find("--extra-experimental-features") == std::string::npos &&
                        remoteCmd.find("nix-shell") == std::string::npos) {
                        log("Adding --extra-experimental-features flag");
                        size_t nixPos = remoteCmd.find("nix ");
                        if (nixPos != std::string::npos) {
                            adjustedCmd = remoteCmd.substr(0, nixPos + 4) + 
                                         "--extra-experimental-features 'nix-command flakes' " + 
                                         remoteCmd.substr(nixPos + 4);
                        }
                    }
                    
                    // Wrap with flake.nix check and fallback to nix-shell
                    log("Wrapping with flake.nix check and nix-shell fallback");
                    std::string wrapper = 
                        "if [ -f flake.nix ]; then "
                        "  echo '[LSP] Found flake.nix, using: " + adjustedCmd + "' >&2; "
                        "  exec " + adjustedCmd + " --background-index; "
                        "else "
                        "  echo '[LSP] No flake.nix found, falling back to nix-shell' >&2; "
                        "  exec nix-shell -p clang-tools --run 'clangd --background-index'; "
                        "fi";
                    fullCommand = sshPrefix + " \"cd '" + workspaceRoot + "' && " + wrapper + "\"";
                } else {
                    // nix-shell or other command, use directly
                    log("Adjusted command: " + adjustedCmd);
                    fullCommand = sshPrefix + " \"cd '" + workspaceRoot + "' && echo '[LSP] Running: " + adjustedCmd + "' >&2 && " + adjustedCmd + " --background-index\"";
                }
            }
        } else {
            // Local execution
            log("Local execution mode");
            fullCommand = command + " --background-index";
        }
        
        log("=== Final command ===");
        log(fullCommand);
        log("=====================");
        
#ifndef _WIN32
        int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
        
        if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
            log("Failed to create pipes");
            return false;
        }
        
        log("Pipes created, forking...");
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
        log("Fork successful, child PID: " + std::to_string(pid));
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Make stdin non-blocking to prevent UI freezes when writing large messages
        int flags = fcntl(stdin_pipe[1], F_GETFL, 0);
        fcntl(stdin_pipe[1], F_SETFL, flags | O_NONBLOCK);
        
        m_process = std::make_unique<UnixProcessHandle>(
            pid, stdin_pipe[1], stdout_pipe[0], stderr_pipe[0]
        );
#else
        // Windows implementation would go here
        log("Windows not yet supported");
        return false;
#endif
        
        m_running = true;
        log("Starting reader, writer, and stderr threads");
        m_readerThread = std::thread(&LspClient::readerThreadFunc, this);
        m_writerThread = std::thread(&LspClient::writerThreadFunc, this);
        m_stderrThread = std::thread(&LspClient::stderrThreadFunc, this);
        
        log("LSP client started successfully");
        return true;
    }
    
    void stop() {
        if (!m_process) return;
        
        log("Stopping LSP server");
        
        if (m_initialized) {
            sendRequest("shutdown", glz::generic{});
        }
        
        m_running = false;
        
        // Wake up writer thread
        m_writeCondition.notify_one();
        
        if (m_writerThread.joinable()) {
            m_writerThread.join();
        }
        
        if (m_readerThread.joinable()) {
            m_readerThread.join();
        }
        
        if (m_stderrThread.joinable()) {
            m_stderrThread.join();
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
    
    /**
     * Send a custom LSP request with a callback.
     * Useful for debugging and querying clangd status.
     */
    void sendCustomRequest(const std::string& method, const glz::generic& params,
                          std::function<void(const glz::generic&)> callback) {
        int id = m_nextId++;
        
        glz::generic msg;
        msg["jsonrpc"] = "2.0";
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
        
        sendMessage(msg);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests[id] = callback;
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
    
    void writerThreadFunc() {
        while (m_running || !m_writeQueue.empty()) {
            std::unique_lock<std::mutex> lock(m_writeMutex);
            m_writeCondition.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !m_writeQueue.empty() || !m_running;
            });
            
            if (m_writeQueue.empty()) continue;
            
            std::string message = m_writeQueue.front();
            m_writeQueue.pop();
            lock.unlock();
            
            // Write to clangd (this may block but it's in background thread)
            if (m_process && !m_process->writeStdin(message.c_str(), message.size())) {
                log("Failed to write to LSP server stdin");
            }
        }
    }
    
    void sendMessage(const glz::generic& msg) {
        if (!m_process) return;
        
        std::string content = glz::write_json(msg).value_or("{}");
        std::string message = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
        
        // Queue the message for the writer thread to send
        {
            std::lock_guard<std::mutex> lock(m_writeMutex);
            m_writeQueue.push(message);
        }
        m_writeCondition.notify_one();
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
    
    void stderrThreadFunc() {
        char buffer[4096];
        std::string lineBuffer;
        
        while (m_running && m_process) {
            int bytesRead = m_process->readStderr(buffer, sizeof(buffer) - 1);
            
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                lineBuffer += buffer;
                
                // Process complete lines
                size_t pos;
                while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                    std::string line = lineBuffer.substr(0, pos);
                    lineBuffer = lineBuffer.substr(pos + 1);
                    
                    // Remove trailing \r if present
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    
                    if (!line.empty()) {
                        log("[stderr] " + line);
                    }
                }
            } else if (bytesRead < 0) {
                // EOF or error - this is normal when process exits
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Log any remaining content in buffer
        if (!lineBuffer.empty()) {
            log("[stderr] " + lineBuffer);
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
            if (method == "textDocument/publishDiagnostics") {
                if (m_diagnosticsCallback) {
                    handleDiagnostics(msg["params"]);
                }
                // Always consume this notification even without callback
            } else if (method == "$/progress") {
                // Background indexing progress notification
                if (msg.contains("params")) {
                    std::string progressJson = glz::write_json(msg["params"]).value_or("{}");
                    log("Progress: " + progressJson);
                }
            } else if (method == "window/logMessage") {
                // Log message from server
                if (msg.contains("params") && msg["params"].is_object()) {
                    auto& params = msg["params"];
                    if (params.contains("message")) {
                        log("Server: " + params["message"].get<std::string>());
                    }
                }
            } else if (method == "window/showMessage") {
                // Status message from server
                if (msg.contains("params") && msg["params"].is_object()) {
                    auto& params = msg["params"];
                    if (params.contains("message")) {
                        log("Status: " + params["message"].get<std::string>());
                    }
                }
            } else if (method.find("$/") == 0) {
                // Silently ignore other $ prefixed notifications (internal protocol extensions)
            } else {
                // Log truly unexpected notifications for debugging
                log("Unhandled notification: " + method);
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
