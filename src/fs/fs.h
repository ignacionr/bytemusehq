#ifndef FS_H
#define FS_H

#include <wx/wx.h>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "../config/config.h"

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#endif

namespace FS {

/**
 * File entry information returned by directory listings.
 */
struct FileEntry {
    wxString name;
    wxString fullPath;
    bool isDirectory;
    int64_t size;       // -1 if unknown
    time_t modTime;     // 0 if unknown
    
    FileEntry() : isDirectory(false), size(-1), modTime(0) {}
    FileEntry(const wxString& n, const wxString& path, bool isDir)
        : name(n), fullPath(path), isDirectory(isDir), size(-1), modTime(0) {}
};

/**
 * Result of a file read operation.
 */
struct ReadResult {
    bool success;
    wxString content;
    wxString error;
    
    static ReadResult Success(const wxString& content) {
        return { true, content, wxEmptyString };
    }
    static ReadResult Error(const wxString& error) {
        return { false, wxEmptyString, error };
    }
};

/**
 * Result of a file write operation.
 */
struct WriteResult {
    bool success;
    wxString error;
    
    static WriteResult Success() {
        return { true, wxEmptyString };
    }
    static WriteResult Error(const wxString& error) {
        return { false, error };
    }
};

/**
 * SSH configuration for remote filesystem access.
 */
struct SshConfig {
    bool enabled = false;
    std::string host;
    int port = 22;
    std::string user;
    std::string identityFile;
    std::string extraOptions;
    int connectionTimeout = 30;
    
    /**
     * Build SSH command prefix for remote operations.
     */
    std::string buildSshPrefix() const {
        if (!enabled || host.empty()) return "";
        
        std::string cmd = "ssh";
        
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
        cmd += " -o BatchMode=yes";
        
        if (!user.empty()) {
            cmd += " " + user + "@" + host;
        } else {
            cmd += " " + host;
        }
        
        return cmd;
    }
    
    /**
     * Build SCP command prefix for file transfers.
     */
    std::string buildScpPrefix() const {
        if (!enabled || host.empty()) return "";
        
        std::string cmd = "scp";
        
        if (!extraOptions.empty()) {
            // Some SSH options may not apply to scp, but most do
            cmd += " " + extraOptions;
        }
        
        if (!identityFile.empty()) {
            cmd += " -i \"" + identityFile + "\"";
        }
        
        if (port != 22) {
            cmd += " -P " + std::to_string(port);  // Note: scp uses -P, not -p
        }
        
        cmd += " -o ConnectTimeout=" + std::to_string(connectionTimeout);
        cmd += " -o BatchMode=yes";
        
        return cmd;
    }
    
    /**
     * Get the remote host specification (user@host or just host).
     */
    std::string getHostSpec() const {
        if (!user.empty()) {
            return user + "@" + host;
        }
        return host;
    }
    
    bool isValid() const {
        return enabled && !host.empty();
    }
    
    /**
     * Expand tilde to actual home directory path via SSH.
     * Returns the expanded path, or the original if expansion fails.
     */
    std::string expandRemotePath(const std::string& path) const {
        if (path.empty() || path[0] != '~') {
            return path;  // No tilde to expand
        }
        
        if (!isValid()) {
            return path;
        }
        
        // Use eval to expand the tilde on the remote side
        std::string cmd = buildSshPrefix() + " \"eval echo " + path + "\" 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return path;
        }
        
        char buffer[1024];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result = buffer;
            // Remove trailing newline
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
        }
        int status = pclose(pipe);
        
        return (result.empty() || status != 0) ? path : result;
    }
    
    /**
     * Load SSH configuration from the app config.
     */
    static SshConfig LoadFromConfig() {
        auto& config = Config::Instance();
        SshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        return ssh;
    }
};


/**
 * Unified filesystem interface for local and remote file operations.
 * 
 * This class provides a consistent API for working with files whether they
 * are on the local machine or on a remote server accessed via SSH.
 */
class Filesystem {
public:
    /**
     * Create a local filesystem instance rooted at the given path.
     */
    static Filesystem Local(const wxString& rootPath = wxGetCwd());
    
    /**
     * Create a remote filesystem instance using the given SSH configuration.
     */
    static Filesystem Remote(const SshConfig& sshConfig, const wxString& remotePath);
    
    /**
     * Create a filesystem instance based on the current app configuration.
     * If SSH is enabled in config, creates a remote filesystem.
     * Otherwise, creates a local filesystem rooted at the current directory.
     */
    static Filesystem FromConfig();
    
    // Default constructor creates local filesystem at cwd
    Filesystem();
    ~Filesystem() = default;
    
    // Copy and move
    Filesystem(const Filesystem&) = default;
    Filesystem& operator=(const Filesystem&) = default;
    Filesystem(Filesystem&&) = default;
    Filesystem& operator=(Filesystem&&) = default;
    
    // --- Properties ---
    
    bool isRemote() const { return m_isRemote; }
    const wxString& rootPath() const { return m_rootPath; }
    const SshConfig& sshConfig() const { return m_sshConfig; }
    std::string sshPrefix() const { return m_sshConfig.buildSshPrefix(); }
    
    // --- Directory operations ---
    
    /**
     * List contents of a directory.
     * @param path Path to the directory (relative to root or absolute).
     * @param includeHidden If true, includes hidden files (starting with '.').
     * @return Vector of file entries, or empty on error.
     */
    std::vector<FileEntry> listDirectory(const wxString& path, bool includeHidden = false) const;
    
    /**
     * Check if a path is a directory.
     */
    bool isDirectory(const wxString& path) const;
    
    /**
     * Check if a path exists.
     */
    bool exists(const wxString& path) const;
    
    // --- File reading ---
    
    /**
     * Read the entire contents of a file.
     */
    ReadResult readFile(const wxString& path) const;
    
    /**
     * Read a specific range of lines from a file.
     * @param path Path to the file.
     * @param startLine First line to read (1-indexed).
     * @param endLine Last line to read (1-indexed, inclusive). -1 for end of file.
     */
    ReadResult readFileLines(const wxString& path, int startLine, int endLine = -1) const;
    
    // --- File writing ---
    
    /**
     * Write content to a file, creating or overwriting.
     */
    WriteResult writeFile(const wxString& path, const wxString& content) const;
    
    /**
     * Append content to a file.
     */
    WriteResult appendFile(const wxString& path, const wxString& content) const;
    
    // --- Path utilities ---
    
    /**
     * Resolve a relative path to an absolute path within this filesystem.
     */
    wxString resolvePath(const wxString& relativePath) const;
    
    /**
     * Get the filename portion of a path.
     */
    static wxString getFilename(const wxString& path);
    
    /**
     * Get the extension of a file (without the dot).
     */
    static wxString getExtension(const wxString& path);
    
    /**
     * Get the directory portion of a path.
     */
    static wxString getDirectory(const wxString& path);

private:
    bool m_isRemote;
    wxString m_rootPath;
    SshConfig m_sshConfig;
    
    // Private constructor - use factory methods
    Filesystem(bool isRemote, const wxString& rootPath, const SshConfig& sshConfig);
    
    // --- Internal helpers ---
    
    std::vector<FileEntry> listDirectoryLocal(const wxString& path, bool includeHidden) const;
    std::vector<FileEntry> listDirectoryRemote(const wxString& path, bool includeHidden) const;
    
    ReadResult readFileLocal(const wxString& path) const;
    ReadResult readFileRemote(const wxString& path) const;
    
    WriteResult writeFileLocal(const wxString& path, const wxString& content) const;
    WriteResult writeFileRemote(const wxString& path, const wxString& content) const;
    
    bool existsLocal(const wxString& path) const;
    bool existsRemote(const wxString& path) const;
    
    bool isDirectoryLocal(const wxString& path) const;
    bool isDirectoryRemote(const wxString& path) const;
};

} // namespace FS

#endif // FS_H
