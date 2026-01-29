#ifndef MCP_TERMINAL_H
#define MCP_TERMINAL_H

#include "mcp.h"
#include <wx/filename.h>
#include <wx/dir.h>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace MCP {

/**
 * Terminal MCP Provider.
 * 
 * Provides tools for the AI to execute shell commands on the system.
 * Automatically selects the appropriate shell based on the runtime platform:
 * - Windows: cmd.exe or PowerShell
 * - macOS/Linux: bash, zsh, or sh
 * 
 * Security: Commands are executed in the configured working directory.
 * There are timeouts and output limits to prevent runaway processes.
 * 
 * Available tools:
 * - terminal_execute: Execute a command and return its output
 * - terminal_get_shell_info: Get information about the available shell
 * - terminal_get_env: Get environment variable value
 * - terminal_which: Find the path of an executable
 */
class TerminalProvider : public Provider {
public:
    TerminalProvider() {
        m_workingDirectory = wxGetCwd().ToStdString();
        detectShell();
    }
    
    explicit TerminalProvider(const std::string& workingDirectory) 
        : m_workingDirectory(workingDirectory) {
        detectShell();
    }
    
    std::string getId() const override {
        return "mcp.terminal";
    }
    
    std::string getName() const override {
        return "Terminal";
    }
    
    std::string getDescription() const override {
        return "Provides shell command execution capabilities";
    }
    
    /**
     * Set the working directory for command execution.
     */
    void setWorkingDirectory(const std::string& path) {
        m_workingDirectory = path;
    }
    
    /**
     * Get the current working directory.
     */
    std::string getWorkingDirectory() const {
        return m_workingDirectory;
    }
    
    /**
     * Set the maximum execution timeout in seconds.
     */
    void setTimeout(int seconds) {
        m_timeoutSeconds = seconds;
    }
    
    /**
     * Set the maximum output size in bytes.
     */
    void setMaxOutputSize(size_t bytes) {
        m_maxOutputBytes = bytes;
    }
    
    std::vector<ToolDefinition> getTools() const override {
        std::vector<ToolDefinition> tools;
        
        // terminal_execute
        {
            ToolDefinition tool;
            tool.name = "terminal_execute";
            tool.description = "Execute a shell command and return its output. "
                             "Commands are run in the configured working directory. "
                             "Use this for running build commands, scripts, or system utilities.";
            tool.parameters = {
                {"command", "string", "The command to execute", true},
                {"working_directory", "string", "Override the working directory for this command (optional)", false},
                {"timeout", "number", "Timeout in seconds (default: 30, max: 300)", false},
                {"shell", "string", "Override the shell to use: 'bash', 'zsh', 'sh', 'cmd', 'powershell' (optional)", false}
            };
            tools.push_back(tool);
        }
        
        // terminal_get_shell_info
        {
            ToolDefinition tool;
            tool.name = "terminal_get_shell_info";
            tool.description = "Get information about the current shell environment including "
                             "the detected shell, platform, and available shells.";
            tool.parameters = {};
            tools.push_back(tool);
        }
        
        // terminal_get_env
        {
            ToolDefinition tool;
            tool.name = "terminal_get_env";
            tool.description = "Get the value of an environment variable.";
            tool.parameters = {
                {"name", "string", "Name of the environment variable", true}
            };
            tools.push_back(tool);
        }
        
        // terminal_which
        {
            ToolDefinition tool;
            tool.name = "terminal_which";
            tool.description = "Find the path of an executable command (like 'which' on Unix or 'where' on Windows).";
            tool.parameters = {
                {"command", "string", "Name of the command to find", true}
            };
            tools.push_back(tool);
        }
        
        // terminal_list_processes
        {
            ToolDefinition tool;
            tool.name = "terminal_list_processes";
            tool.description = "List running processes on the system (limited output).";
            tool.parameters = {
                {"filter", "string", "Optional filter to match process names", false}
            };
            tools.push_back(tool);
        }
        
        return tools;
    }
    
    ToolResult executeTool(const std::string& toolName, const Value& arguments) override {
        if (toolName == "terminal_execute") {
            return executeCommand(arguments);
        } else if (toolName == "terminal_get_shell_info") {
            return getShellInfo(arguments);
        } else if (toolName == "terminal_get_env") {
            return getEnvironmentVariable(arguments);
        } else if (toolName == "terminal_which") {
            return whichCommand(arguments);
        } else if (toolName == "terminal_list_processes") {
            return listProcesses(arguments);
        }
        
        return ToolResult::Error("Unknown tool: " + toolName);
    }

private:
    std::string m_workingDirectory;
    std::string m_defaultShell;
    std::string m_platform;
    int m_timeoutSeconds = 30;
    size_t m_maxOutputBytes = 100000;  // 100KB max output
    
    /**
     * Detect the platform and available shell.
     */
    void detectShell() {
#ifdef _WIN32
        m_platform = "windows";
        // Check for PowerShell first, then fall back to cmd.exe
        if (shellExists("powershell")) {
            m_defaultShell = "powershell";
        } else {
            m_defaultShell = "cmd";
        }
#elif defined(__APPLE__)
        m_platform = "macos";
        // macOS default is zsh since Catalina
        if (shellExists("zsh")) {
            m_defaultShell = "zsh";
        } else if (shellExists("bash")) {
            m_defaultShell = "bash";
        } else {
            m_defaultShell = "sh";
        }
#else
        m_platform = "linux";
        // Linux typically uses bash
        if (shellExists("bash")) {
            m_defaultShell = "bash";
        } else if (shellExists("zsh")) {
            m_defaultShell = "zsh";
        } else {
            m_defaultShell = "sh";
        }
#endif
    }
    
    /**
     * Check if a shell exists on the system.
     */
    bool shellExists(const std::string& shell) const {
#ifdef _WIN32
        std::string cmd = "where " + shell + " >nul 2>&1";
        return system(cmd.c_str()) == 0;
#else
        std::string cmd = "which " + shell + " >/dev/null 2>&1";
        return system(cmd.c_str()) == 0;
#endif
    }
    
    /**
     * Get the shell command prefix for executing a command.
     */
    std::pair<std::string, std::vector<std::string>> getShellCommand(const std::string& shell, const std::string& command) const {
        std::string shellPath;
        std::vector<std::string> args;
        
#ifdef _WIN32
        if (shell == "powershell") {
            shellPath = "powershell.exe";
            args = {"-NoProfile", "-NonInteractive", "-Command", command};
        } else {
            shellPath = "cmd.exe";
            args = {"/C", command};
        }
#else
        if (shell == "zsh") {
            shellPath = "/bin/zsh";
        } else if (shell == "bash") {
            shellPath = "/bin/bash";
        } else {
            shellPath = "/bin/sh";
        }
        args = {"-c", command};
#endif
        
        return {shellPath, args};
    }
    
    /**
     * Execute a shell command and capture its output.
     * Uses popen() which is thread-safe, unlike wxExecute.
     */
    std::tuple<int, std::string, std::string> runCommand(
        const std::string& command,
        const std::string& workDir,
        const std::string& shell,
        int timeoutSecs
    ) const {
        std::string stdout_output;
        std::string stderr_output;
        int exitCode = -1;
        
        std::string shellToUse = shell.empty() ? m_defaultShell : shell;
        std::string effectiveWorkDir = workDir.empty() ? m_workingDirectory : workDir;
        
        // Build the full command with cd and shell wrapper
        std::string fullCommand;
        
#ifdef _WIN32
        // Windows: use cd /d for changing drive and directory
        if (shellToUse == "powershell") {
            // PowerShell command
            fullCommand = "powershell.exe -NoProfile -NonInteractive -Command \"";
            if (!effectiveWorkDir.empty()) {
                fullCommand += "Set-Location '" + effectiveWorkDir + "'; ";
            }
            fullCommand += command + "\" 2>&1";
        } else {
            // cmd.exe command
            fullCommand = "cmd.exe /C \"";
            if (!effectiveWorkDir.empty()) {
                fullCommand += "cd /d \"" + effectiveWorkDir + "\" && ";
            }
            fullCommand += command + "\" 2>&1";
        }
#else
        // Unix: build shell command with cd
        std::string shellPath;
        if (shellToUse == "zsh") {
            shellPath = "/bin/zsh";
        } else if (shellToUse == "bash") {
            shellPath = "/bin/bash";
        } else {
            shellPath = "/bin/sh";
        }
        
        // Escape single quotes in the command
        std::string escapedCommand = command;
        size_t pos = 0;
        while ((pos = escapedCommand.find("'", pos)) != std::string::npos) {
            escapedCommand.replace(pos, 1, "'\\''");
            pos += 4;
        }
        
        // Build command: cd to workdir (if specified) and execute
        if (!effectiveWorkDir.empty()) {
            fullCommand = shellPath + " -c 'cd \"" + effectiveWorkDir + "\" && " + escapedCommand + "' 2>&1";
        } else {
            fullCommand = shellPath + " -c '" + escapedCommand + "' 2>&1";
        }
#endif
        
        // Execute using popen (thread-safe)
        FILE* pipe = popen(fullCommand.c_str(), "r");
        if (!pipe) {
            return {-1, "", "Failed to execute command: popen() failed"};
        }
        
        // Read output
        std::array<char, 4096> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            stdout_output += buffer.data();
            if (stdout_output.size() > m_maxOutputBytes) {
                stdout_output = stdout_output.substr(0, m_maxOutputBytes);
                stdout_output += "\n... (output truncated)";
                break;
            }
        }
        
        // Get exit code
        int status = pclose(pipe);
#ifdef _WIN32
        exitCode = status;
#else
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        } else {
            exitCode = -1;
        }
#endif
        
        // Remove trailing newline if present
        if (!stdout_output.empty() && stdout_output.back() == '\n') {
            stdout_output.pop_back();
        }
        
        return {exitCode, stdout_output, stderr_output};
    }
    
    /**
     * Execute command tool implementation.
     */
    ToolResult executeCommand(const Value& args) {
        if (!args.has("command") || !args["command"].isString()) {
            return ToolResult::Error("Missing required parameter: command");
        }
        
        std::string command = args["command"].asString();
        std::string workDir = args.has("working_directory") ? args["working_directory"].asString() : "";
        std::string shell = args.has("shell") ? args["shell"].asString() : "";
        int timeout = args.has("timeout") ? args["timeout"].asInt() : m_timeoutSeconds;
        
        // Validate timeout
        if (timeout <= 0) timeout = m_timeoutSeconds;
        if (timeout > 300) timeout = 300;  // Max 5 minutes
        
        // Validate shell if specified
        if (!shell.empty()) {
            std::vector<std::string> validShells = {"bash", "zsh", "sh", "cmd", "powershell"};
            bool valid = false;
            for (const auto& s : validShells) {
                if (shell == s) {
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                return ToolResult::Error("Invalid shell: " + shell + ". Valid options: bash, zsh, sh, cmd, powershell");
            }
        }
        
        // Validate working directory if specified
        if (!workDir.empty() && !wxFileName::DirExists(wxString(workDir))) {
            return ToolResult::Error("Working directory does not exist: " + workDir);
        }
        
        auto [exitCode, stdout_out, stderr_out] = runCommand(command, workDir, shell, timeout);
        
        Value result;
        result["exit_code"] = exitCode;
        result["stdout"] = stdout_out;
        result["stderr"] = stderr_out;
        result["success"] = (exitCode == 0);
        result["command"] = command;
        result["shell"] = shell.empty() ? m_defaultShell : shell;
        result["working_directory"] = workDir.empty() ? m_workingDirectory : workDir;
        
        return ToolResult::Success(result);
    }
    
    /**
     * Get shell info tool implementation.
     */
    ToolResult getShellInfo(const Value& /*args*/) {
        Value result;
        result["platform"] = m_platform;
        result["default_shell"] = m_defaultShell;
        result["working_directory"] = m_workingDirectory;
        
        // List available shells
        Value shells;
#ifdef _WIN32
        Value cmdShell;
        cmdShell["name"] = "cmd";
        cmdShell["available"] = true;
        cmdShell["path"] = "cmd.exe";
        shells.push_back(cmdShell);
        
        Value psShell;
        psShell["name"] = "powershell";
        psShell["available"] = shellExists("powershell");
        psShell["path"] = "powershell.exe";
        shells.push_back(psShell);
#else
        Value bashShell;
        bashShell["name"] = "bash";
        bashShell["available"] = shellExists("bash");
        bashShell["path"] = "/bin/bash";
        shells.push_back(bashShell);
        
        Value zshShell;
        zshShell["name"] = "zsh";
        zshShell["available"] = shellExists("zsh");
        zshShell["path"] = "/bin/zsh";
        shells.push_back(zshShell);
        
        Value shShell;
        shShell["name"] = "sh";
        shShell["available"] = true;
        shShell["path"] = "/bin/sh";
        shells.push_back(shShell);
#endif
        
        result["available_shells"] = shells;
        result["timeout_seconds"] = m_timeoutSeconds;
        result["max_output_bytes"] = static_cast<int>(m_maxOutputBytes);
        
        return ToolResult::Success(result);
    }
    
    /**
     * Get environment variable tool implementation.
     */
    ToolResult getEnvironmentVariable(const Value& args) {
        if (!args.has("name") || !args["name"].isString()) {
            return ToolResult::Error("Missing required parameter: name");
        }
        
        std::string name = args["name"].asString();
        
        wxString value;
        bool found = wxGetEnv(wxString(name), &value);
        
        Value result;
        result["name"] = name;
        result["found"] = found;
        result["value"] = found ? value.ToStdString() : "";
        
        return ToolResult::Success(result);
    }
    
    /**
     * Which command tool implementation.
     */
    ToolResult whichCommand(const Value& args) {
        if (!args.has("command") || !args["command"].isString()) {
            return ToolResult::Error("Missing required parameter: command");
        }
        
        std::string cmd = args["command"].asString();
        std::string whichCmd;
        
#ifdef _WIN32
        whichCmd = "where " + cmd;
#else
        whichCmd = "which " + cmd;
#endif
        
        auto [exitCode, stdout_out, stderr_out] = runCommand(whichCmd, "", "", 10);
        
        Value result;
        result["command"] = cmd;
        result["found"] = (exitCode == 0);
        
        if (exitCode == 0) {
            // Trim whitespace from path
            std::string path = stdout_out;
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ')) {
                path.pop_back();
            }
            result["path"] = path;
        } else {
            result["path"] = "";
        }
        
        return ToolResult::Success(result);
    }
    
    /**
     * List processes tool implementation.
     */
    ToolResult listProcesses(const Value& args) {
        std::string filter = args.has("filter") ? args["filter"].asString() : "";
        std::string psCmd;
        
#ifdef _WIN32
        if (filter.empty()) {
            psCmd = "tasklist /FO CSV /NH";
        } else {
            psCmd = "tasklist /FO CSV /NH | findstr /I \"" + filter + "\"";
        }
#else
        if (filter.empty()) {
            psCmd = "ps aux | head -50";  // Limit to 50 processes
        } else {
            psCmd = "ps aux | grep -i \"" + filter + "\" | head -50";
        }
#endif
        
        auto [exitCode, stdout_out, stderr_out] = runCommand(psCmd, "", "", 10);
        
        Value result;
        result["output"] = stdout_out;
        result["filter"] = filter;
        result["success"] = (exitCode == 0);
        
        return ToolResult::Success(result);
    }
};

} // namespace MCP

#endif // MCP_TERMINAL_H
