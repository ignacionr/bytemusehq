#ifndef MCP_FILESYSTEM_H
#define MCP_FILESYSTEM_H

#include "mcp.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/textfile.h>
#include <wx/wfstream.h>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

namespace MCP {

/**
 * SSH configuration for remote filesystem access.
 */
struct FilesystemSshConfig {
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
};

/**
 * Filesystem MCP Provider.
 * 
 * Provides tools for the AI to interact with the filesystem within
 * the currently open workspace/directory. This is designed to be
 * an isolated component that can be integrated with the file explorer widget.
 * 
 * Supports both local and remote (SSH) filesystem access.
 * 
 * Security: All operations are sandboxed to the configured root directory.
 * The AI cannot access files outside this directory.
 * 
 * Available tools:
 * - fs_list_directory: List files and directories
 * - fs_read_file: Read file contents
 * - fs_get_file_info: Get file metadata
 * - fs_search_files: Search for files by name pattern
 * - fs_read_file_lines: Read specific lines from a file
 */
class FilesystemProvider : public Provider {
public:
    FilesystemProvider() {
        // Default to current working directory
        m_rootPath = wxGetCwd().ToStdString();
    }
    
    explicit FilesystemProvider(const std::string& rootPath) 
        : m_rootPath(rootPath) {}
    
    std::string getId() const override {
        return "mcp.filesystem";
    }
    
    std::string getName() const override {
        return "Filesystem";
    }
    
    std::string getDescription() const override {
        return "Provides read-only access to files in the current workspace";
    }
    
    /**
     * Set the root path for filesystem operations.
     * All paths will be relative to this directory.
     */
    void setRootPath(const std::string& path) {
        m_rootPath = path;
    }
    
    /**
     * Get the current root path.
     */
    std::string getRootPath() const {
        return m_rootPath;
    }
    
    /**
     * Configure SSH for remote filesystem access.
     */
    void setSshConfig(const FilesystemSshConfig& config) {
        m_sshConfig = config;
    }
    
    /**
     * Get current SSH configuration.
     */
    FilesystemSshConfig getSshConfig() const {
        return m_sshConfig;
    }
    
    /**
     * Check if remote filesystem access is enabled.
     */
    bool isRemoteFilesystem() const {
        return m_sshConfig.isValid();
    }
    
    std::vector<ToolDefinition> getTools() const override {
        std::vector<ToolDefinition> tools;
        
        // fs_list_directory
        {
            ToolDefinition tool;
            tool.name = "fs_list_directory";
            tool.description = "List files and directories in a given path within the workspace. "
                             "Returns names, types (file/directory), and sizes.";
            tool.parameters = {
                {"path", "string", "Relative path to the directory to list. Use '.' for root.", true},
                {"recursive", "boolean", "If true, list recursively (default: false)", false},
                {"max_depth", "number", "Maximum recursion depth (default: 3)", false}
            };
            tools.push_back(tool);
        }
        
        // fs_read_file
        {
            ToolDefinition tool;
            tool.name = "fs_read_file";
            tool.description = "Read the contents of a file. For large files, consider using "
                             "fs_read_file_lines to read specific sections.";
            tool.parameters = {
                {"path", "string", "Relative path to the file to read", true},
                {"max_size", "number", "Maximum bytes to read (default: 100000)", false}
            };
            tools.push_back(tool);
        }
        
        // fs_read_file_lines
        {
            ToolDefinition tool;
            tool.name = "fs_read_file_lines";
            tool.description = "Read specific lines from a file. Useful for examining parts of large files.";
            tool.parameters = {
                {"path", "string", "Relative path to the file", true},
                {"start_line", "number", "First line to read (1-indexed)", true},
                {"end_line", "number", "Last line to read (inclusive)", true}
            };
            tools.push_back(tool);
        }
        
        // fs_get_file_info
        {
            ToolDefinition tool;
            tool.name = "fs_get_file_info";
            tool.description = "Get metadata about a file or directory including size, modification time, "
                             "and type.";
            tool.parameters = {
                {"path", "string", "Relative path to the file or directory", true}
            };
            tools.push_back(tool);
        }
        
        // fs_search_files
        {
            ToolDefinition tool;
            tool.name = "fs_search_files";
            tool.description = "Search for files matching a name pattern. Supports wildcards (* and ?).";
            tool.parameters = {
                {"pattern", "string", "Filename pattern to search for (e.g., '*.cpp', 'test_*.py')", true},
                {"path", "string", "Directory to search in (default: root)", false},
                {"recursive", "boolean", "Search recursively (default: true)", false}
            };
            tools.push_back(tool);
        }
        
        // fs_grep
        {
            ToolDefinition tool;
            tool.name = "fs_grep";
            tool.description = "Search for text within files. Returns matching lines with file paths and line numbers.";
            tool.parameters = {
                {"query", "string", "Text or pattern to search for", true},
                {"path", "string", "Directory to search in (default: root)", false},
                {"file_pattern", "string", "Only search files matching this pattern (e.g., '*.cpp')", false},
                {"case_sensitive", "boolean", "Case sensitive search (default: false)", false},
                {"max_results", "number", "Maximum number of results (default: 50)", false}
            };
            tools.push_back(tool);
        }
        
        return tools;
    }
    
    ToolResult executeTool(const std::string& toolName, const Value& arguments) override {
        if (toolName == "fs_list_directory") {
            return listDirectory(arguments);
        } else if (toolName == "fs_read_file") {
            return readFile(arguments);
        } else if (toolName == "fs_read_file_lines") {
            return readFileLines(arguments);
        } else if (toolName == "fs_get_file_info") {
            return getFileInfo(arguments);
        } else if (toolName == "fs_search_files") {
            return searchFiles(arguments);
        } else if (toolName == "fs_grep") {
            return grepFiles(arguments);
        }
        
        return ToolResult::Error("Unknown tool: " + toolName);
    }

private:
    std::string m_rootPath;
    FilesystemSshConfig m_sshConfig;
    
    /**
     * Execute a remote command via SSH and return output.
     * Used for remote filesystem operations.
     */
    std::pair<int, std::string> executeRemoteCommand(const std::string& command) const {
        if (!m_sshConfig.isValid()) {
            return {-1, "SSH not configured"};
        }
        
        std::string sshPrefix = m_sshConfig.buildSshPrefix();
        std::string fullCommand = sshPrefix + " \"" + command + "\" 2>&1";
        
        FILE* pipe = popen(fullCommand.c_str(), "r");
        if (!pipe) {
            return {-1, "Failed to execute SSH command"};
        }
        
        std::string output;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        
        int status = pclose(pipe);
#ifdef _WIN32
        int exitCode = status;
#else
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
        
        // Remove trailing newline
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
            output.pop_back();
        }
        
        return {exitCode, output};
    }
    
    /**
     * Resolve a relative path to full path (local or remote).
     * For remote, just combines paths; for local, validates sandbox.
     */
    std::string resolveRemotePath(const std::string& relativePath) const {
        std::string basePath = m_rootPath;
        if (relativePath.empty() || relativePath == ".") {
            return basePath;
        }
        
        // Combine paths, ensuring single separator
        std::string combined = basePath;
        if (!combined.empty() && combined.back() != '/') {
            combined += '/';
        }
        combined += relativePath;
        
        return combined;
    }
    
    /**
     * Resolve a relative path to an absolute path within the sandbox.
     * Returns empty string if path escapes the sandbox.
     */
    std::string resolvePath(const std::string& relativePath) const {
        // For remote filesystems, use simpler path resolution
        if (m_sshConfig.isValid()) {
            return resolveRemotePath(relativePath);
        }
        
        // Handle the relative path properly - it may contain subdirectories
        wxFileName fn;
        if (relativePath.empty() || relativePath == ".") {
            fn.AssignDir(m_rootPath);
        } else {
            // Combine root path with relative path
            wxString fullPathStr = wxString(m_rootPath);
            if (!fullPathStr.EndsWith(wxFileName::GetPathSeparator())) {
                fullPathStr += wxFileName::GetPathSeparator();
            }
            fullPathStr += wxString(relativePath);
            fn.Assign(fullPathStr);
        }
        fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        
        std::string fullPath = fn.GetFullPath().ToStdString();
        
        // Security check: ensure path is within root
        wxFileName rootFn;
        rootFn.AssignDir(m_rootPath);
        rootFn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
        std::string normalizedRoot = rootFn.GetFullPath().ToStdString();
        
        // Ensure the root path ends with separator for proper prefix matching
        if (!normalizedRoot.empty() && normalizedRoot.back() != wxFileName::GetPathSeparator()) {
            normalizedRoot += wxFileName::GetPathSeparator();
        }
        
        // Check if fullPath starts with normalizedRoot, or equals the root (without trailing separator)
        std::string rootWithoutSep = normalizedRoot;
        if (!rootWithoutSep.empty() && rootWithoutSep.back() == wxFileName::GetPathSeparator()) {
            rootWithoutSep.pop_back();
        }
        
        if (fullPath != rootWithoutSep && fullPath.find(normalizedRoot) != 0) {
            return ""; // Path escapes sandbox
        }
        
        return fullPath;
    }
    
    /**
     * Check if a dotfile/directory should be skipped.
     * Allows .vscode but skips .git, .DS_Store, etc.
     */
    bool shouldSkipDotfile(const wxString& filename) const {
        if (!filename.StartsWith(".")) {
            return false;  // Not a dotfile, don't skip
        }
        // Allow these dotfiles/directories
        if (filename == ".vscode" || filename == ".github" || 
            filename == ".gitignore" || filename == ".editorconfig" ||
            filename == ".clang-format" || filename == ".clang-tidy" ||
            filename == ".env" || filename == ".env.local" ||
            filename == ".prettierrc" || filename == ".eslintrc" ||
            filename == ".eslintrc.json" || filename == ".eslintrc.js") {
            return false;  // Allow these
        }
        return true;  // Skip other dotfiles like .git, .DS_Store
    }
    
    /**
     * Convert absolute path to relative path from root.
     */
    std::string toRelativePath(const std::string& absolutePath) const {
        wxFileName fn(absolutePath);
        fn.MakeRelativeTo(m_rootPath);
        return fn.GetFullPath().ToStdString();
    }
    
    /**
     * Check if path is a valid, accessible location within sandbox.
     */
    bool isValidPath(const std::string& path) const {
        if (path.empty()) return false;
        
        // For remote filesystem, check via SSH
        if (m_sshConfig.isValid()) {
            auto [exitCode, _] = executeRemoteCommand("test -e \"" + path + "\"");
            return exitCode == 0;
        }
        
        return wxFileExists(path) || wxDirExists(path);
    }
    
    /**
     * Check if remote path is a directory.
     */
    bool isRemoteDirectory(const std::string& path) const {
        auto [exitCode, _] = executeRemoteCommand("test -d \"" + path + "\"");
        return exitCode == 0;
    }
    
    /**
     * Check if remote path is a file.
     */
    bool isRemoteFile(const std::string& path) const {
        auto [exitCode, _] = executeRemoteCommand("test -f \"" + path + "\"");
        return exitCode == 0;
    }
    
    // ========== Tool Implementations ==========
    
    ToolResult listDirectory(const Value& args) {
        std::string relPath = args.has("path") ? args["path"].asString() : ".";
        bool recursive = args.has("recursive") ? args["recursive"].asBool() : false;
        int maxDepth = args.has("max_depth") ? args["max_depth"].asInt() : 3;
        
        std::string fullPath = resolvePath(relPath);
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        // Remote filesystem listing
        if (m_sshConfig.isValid()) {
            return listDirectoryRemote(fullPath, relPath, recursive, maxDepth);
        }
        
        if (!wxDirExists(fullPath)) {
            return ToolResult::Error("Directory not found: " + relPath);
        }
        
        Value result;
        result["path"] = relPath;
        result["entries"] = listDirectoryRecursive(fullPath, recursive, maxDepth, 0);
        
        return ToolResult::Success(result);
    }
    
    /**
     * List directory contents on remote machine via SSH.
     */
    ToolResult listDirectoryRemote(const std::string& fullPath, const std::string& relPath, 
                                   bool recursive, int maxDepth) {
        // First verify the directory exists
        if (!isRemoteDirectory(fullPath)) {
            return ToolResult::Error("Directory not found or inaccessible: " + relPath);
        }
        
        Value entries = listDirectoryRemoteRecursive(fullPath, relPath, recursive, maxDepth, 0);
        
        Value result;
        result["path"] = relPath;
        result["entries"] = entries;
        result["remote"] = true;
        
        return ToolResult::Success(result);
    }
    
    /**
     * Recursively list directory contents on remote machine via SSH.
     */
    Value listDirectoryRemoteRecursive(const std::string& fullPath, const std::string& relPath,
                                       bool recursive, int maxDepth, int currentDepth) {
        Value entries;
        
        // Use ls with stat-like output for detailed info
        std::string lsCmd = "ls -la \"" + fullPath + "\" 2>/dev/null";
        auto [exitCode, output] = executeRemoteCommand(lsCmd);
        
        if (exitCode != 0) {
            return entries;  // Return empty entries on error
        }
        
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Skip total line and empty lines
            if (line.empty() || line.find("total") == 0) continue;
            
            // Parse ls -la output: permissions links owner group size month day time name
            std::istringstream lineStream(line);
            std::string permissions, links, owner, group, size, month, day, timeStr, name;
            lineStream >> permissions >> links >> owner >> group >> size >> month >> day >> timeStr;
            std::getline(lineStream, name);
            
            // Trim leading whitespace from name
            size_t start = name.find_first_not_of(" \t");
            if (start != std::string::npos) {
                name = name.substr(start);
            }
            
            // Skip . and .. and hidden files (unless allowed)
            if (name.empty() || name == "." || name == "..") continue;
            if (shouldSkipDotfile(wxString(name))) continue;
            
            Value entry;
            entry["name"] = name;
            std::string entryRelPath = relPath.empty() || relPath == "." ? name : relPath + "/" + name;
            entry["path"] = entryRelPath;
            
            if (!permissions.empty() && permissions[0] == 'd') {
                entry["type"] = "directory";
                
                // Recurse into subdirectories if requested
                if (recursive && currentDepth < maxDepth) {
                    std::string childFullPath = fullPath;
                    if (!childFullPath.empty() && childFullPath.back() != '/') {
                        childFullPath += '/';
                    }
                    childFullPath += name;
                    
                    entry["children"] = listDirectoryRemoteRecursive(
                        childFullPath, entryRelPath, true, maxDepth, currentDepth + 1);
                }
            } else {
                entry["type"] = "file";
                try {
                    entry["size"] = std::stod(size);
                } catch (...) {
                    entry["size"] = 0.0;
                }
            }
            
            entries.push_back(entry);
        }
        
        return entries;
    }
    
    Value listDirectoryRecursive(const std::string& path, bool recursive, int maxDepth, int currentDepth) {
        Value entries;
        
        wxDir dir(path);
        if (!dir.IsOpened()) {
            return entries;
        }
        
        wxString filename;
        bool cont = dir.GetFirst(&filename);
        
        while (cont) {
            // Skip certain hidden files but allow .vscode, .github, etc.
            if (!shouldSkipDotfile(filename)) {
                std::string fullPath = wxFileName(path, filename).GetFullPath().ToStdString();
                
                Value entry;
                entry["name"] = filename.ToStdString();
                entry["path"] = toRelativePath(fullPath);
                
                if (wxDir::Exists(fullPath)) {
                    entry["type"] = "directory";
                    
                    if (recursive && currentDepth < maxDepth) {
                        entry["children"] = listDirectoryRecursive(fullPath, true, maxDepth, currentDepth + 1);
                    }
                } else {
                    entry["type"] = "file";
                    wxFileName fn(fullPath);
                    entry["size"] = static_cast<double>(fn.GetSize().GetValue());
                    entry["extension"] = fn.GetExt().ToStdString();
                }
                
                entries.push_back(entry);
            }
            cont = dir.GetNext(&filename);
        }
        
        return entries;
    }
    
    /**
     * Read file contents from remote machine via SSH.
     */
    ToolResult readFileRemote(const std::string& fullPath, const std::string& relPath, int maxSize) {
        // First check if file exists and get size
        auto [statCode, statOut] = executeRemoteCommand("stat -f '%z' \"" + fullPath + "\" 2>/dev/null || stat --format='%s' \"" + fullPath + "\" 2>/dev/null");
        if (statCode != 0) {
            return ToolResult::Error("File not found: " + relPath);
        }
        
        long fileSize = 0;
        try {
            fileSize = std::stol(statOut);
        } catch (...) {
            fileSize = 0;
        }
        
        bool truncated = fileSize > maxSize;
        
        // Read file content via cat, with optional head for truncation
        std::string catCmd;
        if (truncated) {
            catCmd = "head -c " + std::to_string(maxSize) + " \"" + fullPath + "\"";
        } else {
            catCmd = "cat \"" + fullPath + "\"";
        }
        
        auto [exitCode, content] = executeRemoteCommand(catCmd);
        if (exitCode != 0) {
            return ToolResult::Error("Could not read file: " + relPath);
        }
        
        // Check if binary (contains null bytes)
        bool isBinary = content.find('\0') != std::string::npos;
        
        Value result;
        result["path"] = relPath;
        result["size"] = static_cast<double>(fileSize);
        result["truncated"] = truncated;
        result["remote"] = true;
        
        if (isBinary) {
            result["content"] = "[Binary file - content not displayed]";
            result["binary"] = true;
        } else {
            result["content"] = content;
            result["binary"] = false;
        }
        
        return ToolResult::Success(result);
    }
    
    ToolResult readFile(const Value& args) {
        if (!args.has("path")) {
            return ToolResult::Error("Missing required parameter: path");
        }
        
        std::string relPath = args["path"].asString();
        int maxSize = args.has("max_size") ? args["max_size"].asInt() : 100000;
        
        std::string fullPath = resolvePath(relPath);
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        // Remote file reading via SSH
        if (m_sshConfig.isValid()) {
            return readFileRemote(fullPath, relPath, maxSize);
        }
        
        if (!wxFileExists(fullPath)) {
            return ToolResult::Error("File not found: " + relPath);
        }
        
        // Check file size
        wxFileName fn(fullPath);
        wxULongLong fileSize = fn.GetSize();
        if (fileSize == wxInvalidSize) {
            return ToolResult::Error("Could not determine file size");
        }
        
        bool truncated = false;
        size_t readSize = static_cast<size_t>(fileSize.GetValue());
        if (readSize > static_cast<size_t>(maxSize)) {
            readSize = maxSize;
            truncated = true;
        }
        
        // Read file
        std::ifstream file(fullPath, std::ios::binary);
        if (!file) {
            return ToolResult::Error("Could not open file: " + relPath);
        }
        
        std::string content(readSize, '\0');
        file.read(&content[0], readSize);
        content.resize(file.gcount());
        
        // Check if binary
        bool isBinary = false;
        for (char c : content) {
            if (c == '\0' && &c != &content.back()) {
                isBinary = true;
                break;
            }
        }
        
        Value result;
        result["path"] = relPath;
        result["size"] = static_cast<double>(fileSize.GetValue());
        result["truncated"] = truncated;
        
        if (isBinary) {
            result["content"] = "[Binary file - content not displayed]";
            result["binary"] = true;
        } else {
            result["content"] = content;
            result["binary"] = false;
        }
        
        return ToolResult::Success(result);
    }
    
    ToolResult readFileLines(const Value& args) {
        if (!args.has("path") || !args.has("start_line") || !args.has("end_line")) {
            return ToolResult::Error("Missing required parameters: path, start_line, end_line");
        }
        
        std::string relPath = args["path"].asString();
        int startLine = args["start_line"].asInt();
        int endLine = args["end_line"].asInt();
        
        if (startLine < 1 || endLine < startLine) {
            return ToolResult::Error("Invalid line range");
        }
        
        std::string fullPath = resolvePath(relPath);
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        if (!wxFileExists(fullPath)) {
            return ToolResult::Error("File not found: " + relPath);
        }
        
        std::ifstream file(fullPath);
        if (!file) {
            return ToolResult::Error("Could not open file: " + relPath);
        }
        
        std::string content;
        std::string line;
        int lineNum = 0;
        int linesRead = 0;
        
        while (std::getline(file, line)) {
            lineNum++;
            if (lineNum >= startLine && lineNum <= endLine) {
                if (!content.empty()) content += "\n";
                content += line;
                linesRead++;
            }
            if (lineNum > endLine) break;
        }
        
        Value result;
        result["path"] = relPath;
        result["start_line"] = startLine;
        result["end_line"] = std::min(endLine, lineNum);
        result["total_lines"] = lineNum;
        result["lines_read"] = linesRead;
        result["content"] = content;
        
        return ToolResult::Success(result);
    }
    
    ToolResult getFileInfo(const Value& args) {
        if (!args.has("path")) {
            return ToolResult::Error("Missing required parameter: path");
        }
        
        std::string relPath = args["path"].asString();
        std::string fullPath = resolvePath(relPath);
        
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        if (!isValidPath(fullPath)) {
            return ToolResult::Error("Path not found: " + relPath);
        }
        
        wxFileName fn(fullPath);
        
        Value result;
        result["path"] = relPath;
        result["name"] = fn.GetFullName().ToStdString();
        result["exists"] = true;
        
        if (wxDir::Exists(fullPath)) {
            result["type"] = "directory";
            
            // Count children
            wxDir dir(fullPath);
            if (dir.IsOpened()) {
                int fileCount = 0;
                int dirCount = 0;
                wxString filename;
                bool cont = dir.GetFirst(&filename);
                while (cont) {
                    if (!shouldSkipDotfile(filename)) {
                        if (wxDir::Exists(wxFileName(fullPath, filename).GetFullPath())) {
                            dirCount++;
                        } else {
                            fileCount++;
                        }
                    }
                    cont = dir.GetNext(&filename);
                }
                result["file_count"] = fileCount;
                result["directory_count"] = dirCount;
            }
        } else {
            result["type"] = "file";
            result["size"] = static_cast<double>(fn.GetSize().GetValue());
            result["extension"] = fn.GetExt().ToStdString();
            
            // Try to get modification time
            wxDateTime modTime;
            if (fn.GetTimes(nullptr, &modTime, nullptr)) {
                result["modified"] = modTime.FormatISOCombined().ToStdString();
            }
            
            // Count lines for text files
            if (isLikelyTextFile(fn.GetExt().ToStdString())) {
                std::ifstream file(fullPath);
                int lineCount = 0;
                std::string line;
                while (std::getline(file, line)) {
                    lineCount++;
                }
                result["line_count"] = lineCount;
            }
        }
        
        return ToolResult::Success(result);
    }
    
    ToolResult searchFiles(const Value& args) {
        if (!args.has("pattern")) {
            return ToolResult::Error("Missing required parameter: pattern");
        }
        
        std::string pattern = args["pattern"].asString();
        std::string relPath = args.has("path") ? args["path"].asString() : ".";
        bool recursive = args.has("recursive") ? args["recursive"].asBool() : true;
        
        std::string fullPath = resolvePath(relPath);
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        if (!wxDirExists(fullPath)) {
            return ToolResult::Error("Directory not found: " + relPath);
        }
        
        Value results;
        searchFilesRecursive(fullPath, pattern, recursive, results);
        
        Value result;
        result["pattern"] = pattern;
        result["search_path"] = relPath;
        result["matches"] = results;
        result["count"] = static_cast<int>(results.size());
        
        return ToolResult::Success(result);
    }
    
    void searchFilesRecursive(const std::string& path, const std::string& pattern, 
                              bool recursive, Value& results, int maxResults = 100) {
        if (results.size() >= static_cast<size_t>(maxResults)) return;
        
        wxDir dir(path);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        
        // Search files
        bool cont = dir.GetFirst(&filename, wxString(pattern), wxDIR_FILES);
        while (cont && results.size() < static_cast<size_t>(maxResults)) {
            if (!shouldSkipDotfile(filename)) {
                std::string fullPath = wxFileName(path, filename).GetFullPath().ToStdString();
                Value entry;
                entry["name"] = filename.ToStdString();
                entry["path"] = toRelativePath(fullPath);
                entry["type"] = "file";
                wxFileName fn(fullPath);
                entry["size"] = static_cast<double>(fn.GetSize().GetValue());
                results.push_back(entry);
            }
            cont = dir.GetNext(&filename);
        }
        
        // Recurse into directories
        if (recursive) {
            cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
            while (cont && results.size() < static_cast<size_t>(maxResults)) {
                if (!shouldSkipDotfile(filename) && filename != "node_modules") {
                    std::string subPath = wxFileName(path, filename).GetFullPath().ToStdString();
                    searchFilesRecursive(subPath, pattern, true, results, maxResults);
                }
                cont = dir.GetNext(&filename);
            }
        }
    }
    
    ToolResult grepFiles(const Value& args) {
        if (!args.has("query")) {
            return ToolResult::Error("Missing required parameter: query");
        }
        
        std::string query = args["query"].asString();
        std::string relPath = args.has("path") ? args["path"].asString() : ".";
        std::string filePattern = args.has("file_pattern") ? args["file_pattern"].asString() : "*";
        bool caseSensitive = args.has("case_sensitive") ? args["case_sensitive"].asBool() : false;
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 50;
        
        std::string fullPath = resolvePath(relPath);
        if (fullPath.empty()) {
            return ToolResult::Error("Invalid path: access denied");
        }
        
        if (!wxDirExists(fullPath)) {
            return ToolResult::Error("Directory not found: " + relPath);
        }
        
        Value matches;
        std::string searchQuery = caseSensitive ? query : toLower(query);
        
        grepFilesRecursive(fullPath, searchQuery, filePattern, caseSensitive, matches, maxResults);
        
        Value result;
        result["query"] = query;
        result["search_path"] = relPath;
        result["matches"] = matches;
        result["count"] = static_cast<int>(matches.size());
        result["truncated"] = (matches.size() >= static_cast<size_t>(maxResults));
        
        return ToolResult::Success(result);
    }
    
    void grepFilesRecursive(const std::string& path, const std::string& query,
                           const std::string& filePattern, bool caseSensitive,
                           Value& matches, int maxResults) {
        if (matches.size() >= static_cast<size_t>(maxResults)) return;
        
        wxDir dir(path);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        
        // Search in files
        bool cont = dir.GetFirst(&filename, wxString(filePattern), wxDIR_FILES);
        while (cont && matches.size() < static_cast<size_t>(maxResults)) {
            if (!shouldSkipDotfile(filename)) {
                std::string filePath = wxFileName(path, filename).GetFullPath().ToStdString();
                
                // Only search text files
                wxFileName fn(filePath);
                if (isLikelyTextFile(fn.GetExt().ToStdString())) {
                    grepFile(filePath, query, caseSensitive, matches, maxResults);
                }
            }
            cont = dir.GetNext(&filename);
        }
        
        // Recurse into directories
        cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
        while (cont && matches.size() < static_cast<size_t>(maxResults)) {
            if (!shouldSkipDotfile(filename) && filename != "node_modules") {
                std::string subPath = wxFileName(path, filename).GetFullPath().ToStdString();
                grepFilesRecursive(subPath, query, filePattern, caseSensitive, matches, maxResults);
            }
            cont = dir.GetNext(&filename);
        }
    }
    
    void grepFile(const std::string& filePath, const std::string& query,
                  bool caseSensitive, Value& matches, int maxResults) {
        std::ifstream file(filePath);
        if (!file) return;
        
        std::string line;
        int lineNum = 0;
        
        while (std::getline(file, line) && matches.size() < static_cast<size_t>(maxResults)) {
            lineNum++;
            
            std::string searchLine = caseSensitive ? line : toLower(line);
            size_t pos = searchLine.find(query);
            
            if (pos != std::string::npos) {
                Value match;
                match["file"] = toRelativePath(filePath);
                match["line"] = lineNum;
                match["column"] = static_cast<int>(pos + 1);
                
                // Trim long lines
                std::string content = line;
                if (content.length() > 200) {
                    size_t start = pos > 50 ? pos - 50 : 0;
                    content = "..." + content.substr(start, 150) + "...";
                }
                match["content"] = content;
                
                matches.push_back(match);
            }
        }
    }
    
    // ========== Utility Functions ==========
    
    std::string toLower(const std::string& str) const {
        std::string result = str;
        for (char& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
        }
        return result;
    }
    
    bool isLikelyTextFile(const std::string& ext) const {
        static const std::set<std::string> textExtensions = {
            "txt", "md", "markdown", "rst", "json", "xml", "yaml", "yml",
            "html", "htm", "css", "js", "ts", "jsx", "tsx", "vue", "svelte",
            "c", "cpp", "cc", "cxx", "h", "hpp", "hxx",
            "java", "kt", "kts", "scala", "groovy",
            "py", "pyw", "pyx", "pxd", "pxi",
            "rb", "rake", "gemspec",
            "rs", "go", "swift", "m", "mm",
            "php", "pl", "pm", "lua",
            "sh", "bash", "zsh", "fish",
            "sql", "graphql", "gql",
            "r", "R", "rmd", "Rmd",
            "tex", "bib",
            "toml", "ini", "cfg", "conf",
            "cmake", "make", "makefile",
            "dockerfile", "containerfile",
            "gitignore", "gitattributes", "editorconfig",
            "env", "properties",
            "log", "csv", "tsv",
            ""  // Files without extension
        };
        
        std::string lowerExt = toLower(ext);
        return textExtensions.find(lowerExt) != textExtensions.end();
    }
};

} // namespace MCP

#endif // MCP_FILESYSTEM_H
