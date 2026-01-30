#include "fs.h"
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/textfile.h>
#include <sstream>

namespace FS {

// --- Factory methods ---

Filesystem Filesystem::Local(const wxString& rootPath) {
    return Filesystem(false, rootPath, SshConfig());
}

Filesystem Filesystem::Remote(const SshConfig& sshConfig, const wxString& remotePath) {
    wxString expandedPath = wxString(sshConfig.expandRemotePath(remotePath.ToStdString()));
    return Filesystem(true, expandedPath, sshConfig);
}

Filesystem Filesystem::FromConfig() {
    SshConfig ssh = SshConfig::LoadFromConfig();
    if (ssh.isValid()) {
        wxString remotePath = Config::Instance().GetString("ssh.remotePath", "~");
        return Remote(ssh, remotePath);
    } else {
        return Local(wxGetCwd());
    }
}

// --- Constructors ---

Filesystem::Filesystem()
    : m_isRemote(false)
    , m_rootPath(wxGetCwd())
    , m_sshConfig()
{
}

Filesystem::Filesystem(bool isRemote, const wxString& rootPath, const SshConfig& sshConfig)
    : m_isRemote(isRemote)
    , m_rootPath(rootPath)
    , m_sshConfig(sshConfig)
{
}

// --- Directory operations ---

std::vector<FileEntry> Filesystem::listDirectory(const wxString& path, bool includeHidden) const {
    if (m_isRemote) {
        return listDirectoryRemote(path, includeHidden);
    } else {
        return listDirectoryLocal(path, includeHidden);
    }
}

std::vector<FileEntry> Filesystem::listDirectoryLocal(const wxString& path, bool includeHidden) const {
    std::vector<FileEntry> entries;
    
    wxDir dir(path);
    if (!dir.IsOpened()) {
        return entries;
    }
    
    wxString filename;
    bool cont = dir.GetFirst(&filename);
    
    while (cont) {
        // Skip hidden files unless requested
        if (!includeHidden && filename.StartsWith(".")) {
            cont = dir.GetNext(&filename);
            continue;
        }
        
        wxString fullPath = wxFileName(path, filename).GetFullPath();
        
        FileEntry entry;
        entry.name = filename;
        entry.fullPath = fullPath;
        entry.isDirectory = wxDir::Exists(fullPath);
        
        // Get file size for regular files
        if (!entry.isDirectory) {
            wxFile file(fullPath);
            if (file.IsOpened()) {
                entry.size = file.Length();
            }
        }
        
        entries.push_back(entry);
        cont = dir.GetNext(&filename);
    }
    
    return entries;
}

std::vector<FileEntry> Filesystem::listDirectoryRemote(const wxString& path, bool includeHidden) const {
    std::vector<FileEntry> entries;
    
    if (!m_sshConfig.isValid()) {
        return entries;
    }
    
    std::string sshPrefix = m_sshConfig.buildSshPrefix();
    std::string cmd = sshPrefix + " \"ls -la '" + path.ToStdString() + "' 2>/dev/null\" 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return entries;
    }
    
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
        std::string permissions, links, owner, group, sizeStr, month, day, timeStr, name;
        lineStream >> permissions >> links >> owner >> group >> sizeStr >> month >> day >> timeStr;
        std::getline(lineStream, name);
        
        // Trim leading whitespace from name
        size_t start = name.find_first_not_of(" \t");
        if (start != std::string::npos) {
            name = name.substr(start);
        }
        
        // Skip . and ..
        if (name.empty() || name == "." || name == "..") continue;
        
        // Skip hidden files unless requested
        if (!includeHidden && !name.empty() && name[0] == '.') continue;
        
        wxString fullPath = path;
        if (!fullPath.EndsWith("/")) fullPath += "/";
        fullPath += wxString(name);
        
        FileEntry entry;
        entry.name = wxString(name);
        entry.fullPath = fullPath;
        entry.isDirectory = !permissions.empty() && permissions[0] == 'd';
        
        // Parse size
        try {
            entry.size = std::stoll(sizeStr);
        } catch (...) {
            entry.size = -1;
        }
        
        entries.push_back(entry);
    }
    
    return entries;
}

bool Filesystem::isDirectory(const wxString& path) const {
    if (m_isRemote) {
        return isDirectoryRemote(path);
    } else {
        return isDirectoryLocal(path);
    }
}

bool Filesystem::isDirectoryLocal(const wxString& path) const {
    return wxDir::Exists(path);
}

bool Filesystem::isDirectoryRemote(const wxString& path) const {
    if (!m_sshConfig.isValid()) {
        return false;
    }
    
    std::string sshPrefix = m_sshConfig.buildSshPrefix();
    std::string cmd = sshPrefix + " \"test -d \\\"" + path.ToStdString() + "\\\"\" 2>&1";
    int result = system(cmd.c_str());
    return result == 0;
}

bool Filesystem::exists(const wxString& path) const {
    if (m_isRemote) {
        return existsRemote(path);
    } else {
        return existsLocal(path);
    }
}

bool Filesystem::existsLocal(const wxString& path) const {
    return wxFileExists(path) || wxDir::Exists(path);
}

bool Filesystem::existsRemote(const wxString& path) const {
    if (!m_sshConfig.isValid()) {
        return false;
    }
    
    std::string sshPrefix = m_sshConfig.buildSshPrefix();
    std::string cmd = sshPrefix + " \"test -e \\\"" + path.ToStdString() + "\\\"\" 2>&1";
    int result = system(cmd.c_str());
    return result == 0;
}

// --- File reading ---

ReadResult Filesystem::readFile(const wxString& path) const {
    if (m_isRemote) {
        return readFileRemote(path);
    } else {
        return readFileLocal(path);
    }
}

ReadResult Filesystem::readFileLocal(const wxString& path) const {
    wxFile file(path);
    if (!file.IsOpened()) {
        return ReadResult::Error("Could not open file: " + path);
    }
    
    wxString content;
    if (!file.ReadAll(&content)) {
        return ReadResult::Error("Could not read file: " + path);
    }
    
    return ReadResult::Success(content);
}

ReadResult Filesystem::readFileRemote(const wxString& path) const {
    if (!m_sshConfig.isValid()) {
        return ReadResult::Error("SSH not configured");
    }
    
    std::string sshPrefix = m_sshConfig.buildSshPrefix();
    std::string cmd = sshPrefix + " \"cat \\\"" + path.ToStdString() + "\\\"\" 2>&1";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return ReadResult::Error("Could not connect to remote host");
    }
    
    std::string content;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        content += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        return ReadResult::Error(wxString::Format("Could not read remote file: %s (exit code: %d)", path, status));
    }
    
    return ReadResult::Success(wxString(content));
}

ReadResult Filesystem::readFileLines(const wxString& path, int startLine, int endLine) const {
    // Read the full file first
    auto result = readFile(path);
    if (!result.success) {
        return result;
    }
    
    // Split into lines and extract the requested range
    wxArrayString lines = wxSplit(result.content, '\n');
    
    int totalLines = static_cast<int>(lines.GetCount());
    
    // Validate and adjust line numbers (1-indexed)
    if (startLine < 1) startLine = 1;
    if (endLine < 0 || endLine > totalLines) endLine = totalLines;
    if (startLine > totalLines) {
        return ReadResult::Success(wxEmptyString);
    }
    
    // Extract the requested lines
    wxString extracted;
    for (int i = startLine - 1; i < endLine; ++i) {
        if (i > startLine - 1) {
            extracted += "\n";
        }
        extracted += lines[i];
    }
    
    return ReadResult::Success(extracted);
}

// --- File writing ---

WriteResult Filesystem::writeFile(const wxString& path, const wxString& content) const {
    if (m_isRemote) {
        return writeFileRemote(path, content);
    } else {
        return writeFileLocal(path, content);
    }
}

WriteResult Filesystem::writeFileLocal(const wxString& path, const wxString& content) const {
    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) {
        return WriteResult::Error("Could not open file for writing: " + path);
    }
    
    if (!file.Write(content)) {
        return WriteResult::Error("Error writing to file: " + path);
    }
    
    return WriteResult::Success();
}

WriteResult Filesystem::writeFileRemote(const wxString& path, const wxString& content) const {
    if (!m_sshConfig.isValid()) {
        return WriteResult::Error("SSH not configured");
    }
    
    // Write content to a temp file, then scp it to remote
    wxString tempPath = wxFileName::CreateTempFileName("bytemuse_");
    {
        wxFile tempFile(tempPath, wxFile::write);
        if (!tempFile.IsOpened() || !tempFile.Write(content)) {
            wxRemoveFile(tempPath);
            return WriteResult::Error("Could not create temp file for remote write");
        }
    }
    
    std::string scpCmd = m_sshConfig.buildScpPrefix();
    scpCmd += " \"" + tempPath.ToStdString() + "\" " + m_sshConfig.getHostSpec() + ":\"" + path.ToStdString() + "\"";
    
    int result = system(scpCmd.c_str());
    wxRemoveFile(tempPath);
    
    if (result != 0) {
        return WriteResult::Error("Could not write remote file: " + path);
    }
    
    return WriteResult::Success();
}

WriteResult Filesystem::appendFile(const wxString& path, const wxString& content) const {
    if (m_isRemote) {
        // For remote, read existing content, append, and write back
        auto readResult = readFile(path);
        wxString newContent = readResult.success ? readResult.content + content : content;
        return writeFile(path, newContent);
    } else {
        // Local append
        wxFile file(path, wxFile::write_append);
        if (!file.IsOpened()) {
            return WriteResult::Error("Could not open file for appending: " + path);
        }
        
        if (!file.Write(content)) {
            return WriteResult::Error("Error appending to file: " + path);
        }
        
        return WriteResult::Success();
    }
}

// --- Path utilities ---

wxString Filesystem::resolvePath(const wxString& relativePath) const {
    if (wxFileName(relativePath).IsAbsolute()) {
        return relativePath;
    }
    
    wxString base = m_rootPath;
    if (!base.EndsWith("/") && !base.EndsWith("\\")) {
        base += "/";
    }
    
    return base + relativePath;
}

wxString Filesystem::getFilename(const wxString& path) {
    wxFileName fn(path);
    return fn.GetFullName();
}

wxString Filesystem::getExtension(const wxString& path) {
    wxFileName fn(path);
    return fn.GetExt();
}

wxString Filesystem::getDirectory(const wxString& path) {
    wxFileName fn(path);
    return fn.GetPath();
}

} // namespace FS
