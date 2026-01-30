#ifndef MCP_CODE_INDEX_H
#define MCP_CODE_INDEX_H

#include "mcp.h"
#include "../lsp/lsp_client.h"
#include <wx/wx.h>
#include <vector>
#include <map>

namespace MCP {

/**
 * SSH configuration for remote code indexing.
 * When enabled, the code index operates on remote files via the LSP server
 * running over SSH.
 */
struct CodeIndexSshConfig {
    bool enabled = false;
    std::string host;
    std::string remotePath;  // Base path on remote machine
    
    bool isValid() const {
        return enabled && !host.empty();
    }
};

/**
 * Code Index MCP Provider.
 * 
 * Exposes the workspace code index (symbols from clangd) to the AI.
 * This allows the AI to understand code structure, find definitions,
 * and navigate the codebase intelligently.
 * 
 * Supports remote code indexing when the LSP server runs over SSH.
 * 
 * Available tools:
 * - code_search_symbols: Search for symbols by name
 * - code_list_symbols: List all symbols in a file
 * - code_get_symbol_info: Get detailed info about a symbol
 * - code_list_functions: List all functions in workspace
 * - code_list_classes: List all classes/structs in workspace
 * - code_get_index_status: Get indexing status
 */
class CodeIndexProvider : public Provider {
public:
    using SymbolEntry = std::pair<std::string, LspDocumentSymbol>;
    using SymbolList = std::vector<SymbolEntry>;
    using SymbolSearchFn = std::function<SymbolList(const std::string&)>;
    using FileSymbolsFn = std::function<std::vector<LspDocumentSymbol>(const std::string&)>;
    using AllSymbolsFn = std::function<SymbolList()>;  // Returns copy, not reference
    using SymbolsByKindFn = std::function<SymbolList(LspSymbolKind)>;
    using IndexStatusFn = std::function<std::tuple<bool, size_t, size_t>()>; // (complete, files, symbols)

    CodeIndexProvider() = default;
    
    std::string getId() const override {
        return "mcp.codeindex";
    }
    
    std::string getName() const override {
        return "Code Index";
    }
    
    std::string getDescription() const override {
        return "Provides access to code symbols and structure from the workspace";
    }
    
    /**
     * Set the callbacks to access the code index.
     * These callbacks connect to the SymbolsWidget.
     */
    void setSearchCallback(SymbolSearchFn fn) { m_searchFn = fn; }
    void setFileSymbolsCallback(FileSymbolsFn fn) { m_fileSymbolsFn = fn; }
    void setAllSymbolsCallback(AllSymbolsFn fn) { m_allSymbolsFn = fn; }
    void setSymbolsByKindCallback(SymbolsByKindFn fn) { m_symbolsByKindFn = fn; }
    void setIndexStatusCallback(IndexStatusFn fn) { m_indexStatusFn = fn; }
    
    /**
     * Configure SSH for remote code indexing.
     * Note: The actual LSP server SSH configuration is set on the LspClient.
     * This config is used for file path resolution in results.
     */
    void setSshConfig(const CodeIndexSshConfig& config) {
        m_sshConfig = config;
    }
    
    /**
     * Get current SSH configuration.
     */
    CodeIndexSshConfig getSshConfig() const {
        return m_sshConfig;
    }
    
    /**
     * Check if remote indexing is enabled.
     */
    bool isRemoteIndexing() const {
        return m_sshConfig.isValid();
    }
    
    std::vector<ToolDefinition> getTools() const override {
        std::vector<ToolDefinition> tools;
        
        // code_search_symbols
        {
            ToolDefinition tool;
            tool.name = "code_search_symbols";
            tool.description = "Search for code symbols (functions, classes, variables, etc.) by name. "
                             "Returns matching symbols with their file paths and line numbers.";
            tool.parameters = {
                {"query", "string", "The symbol name or partial name to search for", true},
                {"max_results", "number", "Maximum number of results to return (default: 20)", false}
            };
            tools.push_back(tool);
        }
        
        // code_list_file_symbols
        {
            ToolDefinition tool;
            tool.name = "code_list_file_symbols";
            tool.description = "List all symbols defined in a specific file. "
                             "Returns functions, classes, methods, variables, etc.";
            tool.parameters = {
                {"path", "string", "The file path (relative to workspace root)", true}
            };
            tools.push_back(tool);
        }
        
        // code_list_functions
        {
            ToolDefinition tool;
            tool.name = "code_list_functions";
            tool.description = "List all functions/methods in the workspace. "
                             "Useful for getting an overview of available functionality.";
            tool.parameters = {
                {"max_results", "number", "Maximum number of results (default: 50)", false}
            };
            tools.push_back(tool);
        }
        
        // code_list_classes
        {
            ToolDefinition tool;
            tool.name = "code_list_classes";
            tool.description = "List all classes and structs in the workspace. "
                             "Useful for understanding the code architecture.";
            tool.parameters = {
                {"max_results", "number", "Maximum number of results (default: 50)", false}
            };
            tools.push_back(tool);
        }
        
        // code_get_index_status
        {
            ToolDefinition tool;
            tool.name = "code_get_index_status";
            tool.description = "Get the current status of the code index. "
                             "Shows whether indexing is complete and how many files/symbols are indexed.";
            tool.parameters = {};
            tools.push_back(tool);
        }
        
        return tools;
    }
    
    ToolResult executeTool(const std::string& toolName, const Value& arguments) override {
        if (toolName == "code_search_symbols") {
            return searchSymbols(arguments);
        } else if (toolName == "code_list_file_symbols") {
            return listFileSymbols(arguments);
        } else if (toolName == "code_list_functions") {
            return listFunctions(arguments);
        } else if (toolName == "code_list_classes") {
            return listClasses(arguments);
        } else if (toolName == "code_get_index_status") {
            return getIndexStatus(arguments);
        }
        
        return ToolResult::Error("Unknown tool: " + toolName);
    }

private:
    SymbolSearchFn m_searchFn;
    FileSymbolsFn m_fileSymbolsFn;
    AllSymbolsFn m_allSymbolsFn;
    SymbolsByKindFn m_symbolsByKindFn;
    IndexStatusFn m_indexStatusFn;
    CodeIndexSshConfig m_sshConfig;
    
    /**
     * Convert symbol kind to human-readable string.
     */
    std::string symbolKindToString(LspSymbolKind kind) {
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
            case LspSymbolKind::EnumMember: return "enum_member";
            case LspSymbolKind::Event: return "event";
            case LspSymbolKind::Operator: return "operator";
            case LspSymbolKind::TypeParameter: return "type_parameter";
            default: return "symbol";
        }
    }
    
    /**
     * Convert a symbol to a Value object for JSON serialization.
     */
    Value symbolToValue(const std::string& filePath, const LspDocumentSymbol& symbol) {
        std::map<std::string, Value> obj;
        obj["name"] = symbol.name;
        obj["kind"] = symbolKindToString(symbol.kind);
        obj["file"] = filePath;
        obj["line"] = symbol.selectionRange.start.line + 1; // 1-indexed for humans
        obj["column"] = symbol.selectionRange.start.character + 1;
        
        if (!symbol.detail.empty()) {
            obj["detail"] = symbol.detail;
        }
        
        return Value(obj);
    }
    
    ToolResult searchSymbols(const Value& arguments) {
        if (!m_searchFn) {
            return ToolResult::Error("Code index not available");
        }
        
        std::string query = arguments["query"].asString();
        if (query.empty()) {
            return ToolResult::Error("Query parameter is required");
        }
        
        int maxResults = arguments.has("max_results") ? arguments["max_results"].asInt() : 20;
        
        auto results = m_searchFn(query);
        
        std::vector<Value> symbols;
        int count = 0;
        for (const auto& [filePath, symbol] : results) {
            if (count++ >= maxResults) break;
            symbols.push_back(symbolToValue(filePath, symbol));
        }
        
        std::map<std::string, Value> result;
        result["query"] = query;
        result["count"] = static_cast<int>(symbols.size());
        result["symbols"] = Value(symbols);
        
        return ToolResult::Success(Value(result));
    }
    
    ToolResult listFileSymbols(const Value& arguments) {
        if (!m_fileSymbolsFn) {
            return ToolResult::Error("Code index not available");
        }
        
        std::string path = arguments["path"].asString();
        if (path.empty()) {
            return ToolResult::Error("Path parameter is required");
        }
        
        auto fileSymbols = m_fileSymbolsFn(path);
        
        std::vector<Value> symbols;
        for (const auto& symbol : fileSymbols) {
            symbols.push_back(symbolToValue(path, symbol));
        }
        
        std::map<std::string, Value> result;
        result["file"] = path;
        result["count"] = static_cast<int>(symbols.size());
        result["symbols"] = Value(symbols);
        
        return ToolResult::Success(Value(result));
    }
    
    ToolResult listFunctions(const Value& arguments) {
        if (!m_symbolsByKindFn) {
            return ToolResult::Error("Code index not available");
        }
        
        int maxResults = arguments.has("max_results") ? arguments["max_results"].asInt() : 50;
        
        // Get functions and methods
        auto functions = m_symbolsByKindFn(LspSymbolKind::Function);
        auto methods = m_symbolsByKindFn(LspSymbolKind::Method);
        
        std::vector<Value> symbols;
        int count = 0;
        
        for (const auto& [filePath, symbol] : functions) {
            if (count++ >= maxResults) break;
            symbols.push_back(symbolToValue(filePath, symbol));
        }
        
        for (const auto& [filePath, symbol] : methods) {
            if (count++ >= maxResults) break;
            symbols.push_back(symbolToValue(filePath, symbol));
        }
        
        std::map<std::string, Value> result;
        result["count"] = static_cast<int>(symbols.size());
        result["symbols"] = Value(symbols);
        
        return ToolResult::Success(Value(result));
    }
    
    ToolResult listClasses(const Value& arguments) {
        if (!m_symbolsByKindFn) {
            return ToolResult::Error("Code index not available");
        }
        
        int maxResults = arguments.has("max_results") ? arguments["max_results"].asInt() : 50;
        
        // Get classes and structs
        auto classes = m_symbolsByKindFn(LspSymbolKind::Class);
        auto structs = m_symbolsByKindFn(LspSymbolKind::Struct);
        
        std::vector<Value> symbols;
        int count = 0;
        
        for (const auto& [filePath, symbol] : classes) {
            if (count++ >= maxResults) break;
            symbols.push_back(symbolToValue(filePath, symbol));
        }
        
        for (const auto& [filePath, symbol] : structs) {
            if (count++ >= maxResults) break;
            symbols.push_back(symbolToValue(filePath, symbol));
        }
        
        std::map<std::string, Value> result;
        result["count"] = static_cast<int>(symbols.size());
        result["symbols"] = Value(symbols);
        
        return ToolResult::Success(Value(result));
    }
    
    ToolResult getIndexStatus(const Value& arguments) {
        if (!m_indexStatusFn) {
            return ToolResult::Error("Code index not available");
        }
        
        auto [complete, fileCount, symbolCount] = m_indexStatusFn();
        
        std::map<std::string, Value> result;
        result["indexing_complete"] = complete;
        result["indexed_files"] = static_cast<int>(fileCount);
        result["indexed_symbols"] = static_cast<int>(symbolCount);
        result["status"] = complete ? "ready" : "indexing";
        
        return ToolResult::Success(Value(result));
    }
};

} // namespace MCP

#endif // MCP_CODE_INDEX_H
