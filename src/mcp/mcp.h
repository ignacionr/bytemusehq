#ifndef MCP_H
#define MCP_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>

#include <wx/log.h>

namespace MCP {

/**
 * JSON-like value type for MCP parameters and results.
 * A simple variant type to represent JSON values without external dependencies.
 */
class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    
    Value() : m_type(Type::Null) {}
    Value(bool b) : m_type(Type::Bool), m_bool(b) {}
    Value(int n) : m_type(Type::Number), m_number(static_cast<double>(n)) {}
    Value(double n) : m_type(Type::Number), m_number(n) {}
    Value(const std::string& s) : m_type(Type::String), m_string(s) {}
    Value(const char* s) : m_type(Type::String), m_string(s) {}
    Value(const std::vector<Value>& arr) : m_type(Type::Array), m_array(arr) {}
    Value(const std::map<std::string, Value>& obj) : m_type(Type::Object), m_object(obj) {}
    
    Type type() const { return m_type; }
    
    bool isNull() const { return m_type == Type::Null; }
    bool isBool() const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray() const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }
    
    bool asBool() const { return m_bool; }
    double asNumber() const { return m_number; }
    int asInt() const { return static_cast<int>(m_number); }
    const std::string& asString() const { return m_string; }
    const std::vector<Value>& asArray() const { return m_array; }
    const std::map<std::string, Value>& asObject() const { return m_object; }
    
    // Object accessors
    bool has(const std::string& key) const {
        return m_type == Type::Object && m_object.find(key) != m_object.end();
    }
    
    const Value& operator[](const std::string& key) const {
        static Value null;
        if (m_type != Type::Object) return null;
        auto it = m_object.find(key);
        return it != m_object.end() ? it->second : null;
    }
    
    Value& operator[](const std::string& key) {
        if (m_type != Type::Object) {
            m_type = Type::Object;
            m_object.clear();
        }
        return m_object[key];
    }
    
    // Array accessors
    size_t size() const {
        if (m_type == Type::Array) return m_array.size();
        if (m_type == Type::Object) return m_object.size();
        return 0;
    }
    
    const Value& operator[](size_t index) const {
        static Value null;
        if (m_type != Type::Array || index >= m_array.size()) return null;
        return m_array[index];
    }
    
    void push_back(const Value& v) {
        if (m_type != Type::Array) {
            m_type = Type::Array;
            m_array.clear();
        }
        m_array.push_back(v);
    }
    
    // JSON serialization
    std::string toJson() const {
        switch (m_type) {
            case Type::Null: return "null";
            case Type::Bool: return m_bool ? "true" : "false";
            case Type::Number: {
                char buf[64];
                if (m_number == static_cast<int>(m_number)) {
                    snprintf(buf, sizeof(buf), "%d", static_cast<int>(m_number));
                } else {
                    snprintf(buf, sizeof(buf), "%g", m_number);
                }
                return buf;
            }
            case Type::String: return "\"" + escapeString(m_string) + "\"";
            case Type::Array: {
                std::string s = "[";
                for (size_t i = 0; i < m_array.size(); ++i) {
                    if (i > 0) s += ",";
                    s += m_array[i].toJson();
                }
                return s + "]";
            }
            case Type::Object: {
                std::string s = "{";
                bool first = true;
                for (const auto& [k, v] : m_object) {
                    if (!first) s += ",";
                    first = false;
                    s += "\"" + escapeString(k) + "\":" + v.toJson();
                }
                return s + "}";
            }
        }
        return "null";
    }
    
private:
    Type m_type;
    bool m_bool = false;
    double m_number = 0;
    std::string m_string;
    std::vector<Value> m_array;
    std::map<std::string, Value> m_object;
    
    static std::string escapeString(const std::string& s) {
        std::string result;
        result.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }
};

/**
 * Parameter schema for a tool.
 */
struct ParameterSchema {
    std::string name;
    std::string type;  // "string", "number", "boolean", "array", "object"
    std::string description;
    bool required = false;
    std::vector<std::string> enumValues;  // For enum types
};

/**
 * Definition of a tool that can be called by the AI.
 */
struct ToolDefinition {
    std::string name;
    std::string description;
    std::vector<ParameterSchema> parameters;
    
    /**
     * Convert to Gemini function declaration JSON format.
     */
    std::string toGeminiFunctionJson() const {
        std::string json = "{";
        json += "\"name\":\"" + name + "\",";
        json += "\"description\":" + Value(description).toJson() + ",";
        json += "\"parameters\":{";
        json += "\"type\":\"object\",";
        json += "\"properties\":{";
        
        bool first = true;
        std::string requiredList;
        for (const auto& param : parameters) {
            if (!first) json += ",";
            first = false;
            
            json += "\"" + param.name + "\":{";
            json += "\"type\":\"" + param.type + "\",";
            json += "\"description\":" + Value(param.description).toJson();
            
            if (!param.enumValues.empty()) {
                json += ",\"enum\":[";
                for (size_t i = 0; i < param.enumValues.size(); ++i) {
                    if (i > 0) json += ",";
                    json += "\"" + param.enumValues[i] + "\"";
                }
                json += "]";
            }
            json += "}";
            
            if (param.required) {
                if (!requiredList.empty()) requiredList += ",";
                requiredList += "\"" + param.name + "\"";
            }
        }
        
        json += "}";
        if (!requiredList.empty()) {
            json += ",\"required\":[" + requiredList + "]";
        }
        json += "}}";
        
        return json;
    }
};

/**
 * Result of a tool call.
 */
struct ToolResult {
    bool success = false;
    Value result;
    std::string error;
    
    static ToolResult Success(const Value& value) {
        return {true, value, ""};
    }
    
    static ToolResult Error(const std::string& msg) {
        return {false, Value(), msg};
    }
};

/**
 * A tool call requested by the AI.
 */
struct ToolCall {
    std::string id;        // Unique ID for this call
    std::string name;      // Tool name
    Value arguments;       // Arguments as parsed JSON
};

/**
 * Base class for MCP providers.
 * Each provider offers a set of tools that can be used by the AI.
 */
class Provider {
public:
    virtual ~Provider() = default;
    
    /**
     * Get the unique ID of this provider.
     */
    virtual std::string getId() const = 0;
    
    /**
     * Get a human-readable name for this provider.
     */
    virtual std::string getName() const = 0;
    
    /**
     * Get a description of what this provider does.
     */
    virtual std::string getDescription() const = 0;
    
    /**
     * Get the list of tools this provider offers.
     */
    virtual std::vector<ToolDefinition> getTools() const = 0;
    
    /**
     * Execute a tool call.
     * @param toolName The name of the tool to call
     * @param arguments The arguments for the tool
     * @return The result of the tool call
     */
    virtual ToolResult executeTool(const std::string& toolName, const Value& arguments) = 0;
    
    /**
     * Check if this provider is currently enabled/available.
     */
    virtual bool isEnabled() const { return true; }
    
    /**
     * Enable or disable this provider.
     */
    virtual void setEnabled(bool enabled) { m_enabled = enabled; }

protected:
    bool m_enabled = true;
};

/**
 * Registry for MCP providers.
 * Manages all available providers and routes tool calls to the appropriate provider.
 */
class Registry {
public:
    static Registry& Instance() {
        static Registry instance;
        return instance;
    }
    
    /**
     * Register a provider.
     */
    void registerProvider(std::shared_ptr<Provider> provider) {
        m_providers[provider->getId()] = provider;
    }
    
    /**
     * Unregister a provider.
     */
    void unregisterProvider(const std::string& id) {
        m_providers.erase(id);
    }
    
    /**
     * Get a provider by ID.
     */
    std::shared_ptr<Provider> getProvider(const std::string& id) const {
        auto it = m_providers.find(id);
        return it != m_providers.end() ? it->second : nullptr;
    }
    
    /**
     * Get all registered providers.
     */
    std::vector<std::shared_ptr<Provider>> getProviders() const {
        std::vector<std::shared_ptr<Provider>> result;
        for (const auto& [id, provider] : m_providers) {
            result.push_back(provider);
        }
        return result;
    }
    
    /**
     * Get all enabled providers.
     */
    std::vector<std::shared_ptr<Provider>> getEnabledProviders() const {
        std::vector<std::shared_ptr<Provider>> result;
        for (const auto& [id, provider] : m_providers) {
            if (provider->isEnabled()) {
                result.push_back(provider);
            }
        }
        return result;
    }
    
    /**
     * Get all tools from all enabled providers.
     */
    std::vector<ToolDefinition> getAllTools() const {
        std::vector<ToolDefinition> tools;
        for (const auto& provider : getEnabledProviders()) {
            auto providerTools = provider->getTools();
            tools.insert(tools.end(), providerTools.begin(), providerTools.end());
        }
        return tools;
    }
    
    /**
     * Execute a tool call, routing to the appropriate provider.
     */
    ToolResult executeTool(const std::string& toolName, const Value& arguments) {
        // Find provider that has this tool
        for (const auto& provider : getEnabledProviders()) {
            for (const auto& tool : provider->getTools()) {
                if (tool.name == toolName) {
                    return provider->executeTool(toolName, arguments);
                }
            }
        }
        return ToolResult::Error("Tool not found: " + toolName);
    }
    
    /**
     * Build the tools JSON for Gemini API.
     */
    std::string buildGeminiToolsJson() const {
        auto tools = getAllTools();
        wxLogDebug("MCP: buildGeminiToolsJson() - %zu tools from %zu enabled providers",
                   tools.size(), getEnabledProviders().size());
        
        // Log all registered providers and their status
        for (const auto& [id, provider] : m_providers) {
            wxLogDebug("MCP: Provider '%s' (%s) - enabled: %s, tools: %zu",
                       id.c_str(), provider->getName().c_str(),
                       provider->isEnabled() ? "yes" : "no",
                       provider->getTools().size());
        }
        
        if (tools.empty()) return "";
        
        std::string json = "\"tools\":[{\"functionDeclarations\":[";
        bool first = true;
        for (const auto& tool : tools) {
            if (!first) json += ",";
            first = false;
            json += tool.toGeminiFunctionJson();
        }
        json += "]}]";
        return json;
    }
    
    /**
     * Generate a human-readable description of available tools for the system instruction.
     * Groups tools by provider and lists them with their descriptions.
     */
    std::string generateToolsDescription() const {
        auto providers = getEnabledProviders();
        if (providers.empty()) return "";
        
        std::string description = "You have access to the user's workspace through several tools:\n\n";
        
        for (const auto& provider : providers) {
            auto tools = provider->getTools();
            if (tools.empty()) continue;
            
            // Provider header
            description += provider->getName() + " TOOLS:\n";
            
            // List each tool
            for (const auto& tool : tools) {
                description += "- " + tool.name + ": " + tool.description + "\n";
            }
            description += "\n";
        }
        
        // Mention disabled providers that could be enabled
        for (const auto& [id, provider] : m_providers) {
            if (!provider->isEnabled()) {
                description += "Note: " + provider->getName() + " tools are available but not currently configured. ";
                description += "The user can enable them by configuring the appropriate settings.\n";
            }
        }
        
        description += "\nWhen the user asks about their code, project structure, or file contents, "
                      "USE THESE TOOLS to read and explore their files. Don't say you can't access files - you can! "
                      "When the user asks you to run commands, build code, or execute scripts, use the terminal tools.";
        
        return description;
    }

private:
    Registry() = default;
    std::map<std::string, std::shared_ptr<Provider>> m_providers;
};

} // namespace MCP

#endif // MCP_H
