#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include "../http/http_client.h"
#include "../config/config.h"
#include "../mcp/mcp.h"
#include <wx/log.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

namespace AI {

/**
 * Message role in a conversation.
 */
enum class MessageRole {
    User,
    Model,
    System
};

/**
 * A single message in a conversation.
 */
struct ChatMessage {
    MessageRole role;
    std::string content;
    
    ChatMessage() = default;
    ChatMessage(MessageRole r, const std::string& c) : role(r), content(c) {}
    
    std::string roleString() const {
        switch (role) {
            case MessageRole::User: return "user";
            case MessageRole::Model: return "model";
            case MessageRole::System: return "user"; // Gemini uses user for system prompts
            default: return "user";
        }
    }
};

/**
 * Result of an AI API call.
 */
struct GeminiResponse {
    std::string text;           // Generated text
    bool success = false;       // Whether the call succeeded
    std::string error;          // Error message if failed
    long httpCode = 0;          // HTTP status code
    int promptTokens = 0;       // Tokens used in prompt
    int completionTokens = 0;   // Tokens in completion
    
    // Function calling support
    bool hasFunctionCall = false;
    std::string functionName;
    std::string functionArgs;   // JSON string of arguments
    
    bool isOk() const { return success && error.empty(); }
    bool needsFunctionCall() const { return success && hasFunctionCall; }
};

/**
 * AI provider type - determines API format and authentication method.
 */
enum class AIProvider {
    Gemini,     // Google Gemini API (key in URL parameter)
    Cortex      // Cortex/OpenAI-compatible API (Bearer token header)
};

/**
 * Configuration for AI API calls.
 */
struct GeminiConfig {
    // Provider settings
    AIProvider provider = AIProvider::Gemini;
    std::string baseUrl;        // Custom base URL (empty = use default for provider)
    std::string apiKey;
    
    // Model settings
    std::string model = "gemini-1.5-flash";
    float temperature = 0.7f;
    int maxOutputTokens = 2048;
    float topP = 0.95f;
    int topK = 40;
    std::string systemInstruction;
    
    // MCP/Function calling settings
    bool enableMCP = true;      // Enable MCP tool calling
    int maxToolCalls = 5;       // Maximum tool calls per response
    
    // Safety settings - block thresholds (Gemini only)
    // Options: BLOCK_NONE, BLOCK_ONLY_HIGH, BLOCK_MEDIUM_AND_ABOVE, BLOCK_LOW_AND_ABOVE
    std::string safetyThreshold = "BLOCK_MEDIUM_AND_ABOVE";
    
    // Get the effective base URL for the current provider
    std::string getEffectiveBaseUrl() const {
        if (!baseUrl.empty()) return baseUrl;
        switch (provider) {
            case AIProvider::Gemini:
                return "https://generativelanguage.googleapis.com/v1beta";
            case AIProvider::Cortex:
                return ""; // Must be configured
            default:
                return "";
        }
    }
    
    // Get provider name as string
    std::string providerName() const {
        switch (provider) {
            case AIProvider::Gemini: return "gemini";
            case AIProvider::Cortex: return "cortex";
            default: return "unknown";
        }
    }
    
    // Parse provider from string
    static AIProvider parseProvider(const std::string& name) {
        if (name == "cortex") return AIProvider::Cortex;
        return AIProvider::Gemini; // Default
    }
};

/**
 * Callback for streaming responses.
 * Called with each chunk of text as it arrives.
 * Return false to cancel the stream.
 */
using StreamCallback = std::function<bool(const std::string& chunk)>;

/**
 * Google Gemini API client.
 * 
 * Provides a clean interface for interacting with Gemini's generative AI API.
 * Supports both single-turn and multi-turn (chat) conversations.
 * 
 * Usage:
 * @code
 * auto& client = GeminiClient::Instance();
 * client.SetApiKey("your-api-key");
 * 
 * // Single prompt
 * auto response = client.Generate("Explain quantum computing");
 * 
 * // Chat conversation
 * client.StartConversation();
 * client.AddMessage(MessageRole::User, "Hello!");
 * auto reply = client.SendMessage("What's the weather like?");
 * @endcode
 */
class GeminiClient {
public:
    /**
     * Get the singleton instance.
     */
    static GeminiClient& Instance() {
        static GeminiClient instance;
        return instance;
    }
    
    // Prevent copying
    GeminiClient(const GeminiClient&) = delete;
    GeminiClient& operator=(const GeminiClient&) = delete;
    
    // ========== Configuration ==========
    
    /**
     * Set the API key for authentication.
     */
    void SetApiKey(const std::string& apiKey) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.apiKey = apiKey;
    }
    
    /**
     * Get the current API key.
     */
    std::string GetApiKey() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.apiKey;
    }
    
    /**
     * Check if an API key is configured.
     */
    bool HasApiKey() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_config.apiKey.empty();
    }
    
    /**
     * Set the AI provider (Gemini or Cortex).
     */
    void SetProvider(AIProvider provider) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.provider = provider;
    }
    
    /**
     * Get the current AI provider.
     */
    AIProvider GetProvider() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.provider;
    }
    
    /**
     * Set custom base URL (for Cortex or custom endpoints).
     */
    void SetBaseUrl(const std::string& url) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.baseUrl = url;
    }
    
    /**
     * Get the current base URL.
     */
    std::string GetBaseUrl() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.baseUrl;
    }
    
    /**
     * Set the model to use (e.g., "gemini-1.5-flash", "gemini-1.5-pro").
     */
    void SetModel(const std::string& model) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.model = model;
    }
    
    /**
     * Get the current model.
     */
    std::string GetModel() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.model;
    }
    
    /**
     * Set generation temperature (0.0 to 2.0).
     */
    void SetTemperature(float temp) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.temperature = std::max(0.0f, std::min(2.0f, temp));
    }
    
    /**
     * Set maximum output tokens.
     */
    void SetMaxOutputTokens(int tokens) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.maxOutputTokens = tokens;
    }
    
    /**
     * Set system instruction (prepended to conversations).
     */
    void SetSystemInstruction(const std::string& instruction) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.systemInstruction = instruction;
    }
    
    /**
     * Get the full configuration.
     */
    GeminiConfig GetConfig() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config;
    }
    
    /**
     * Set the full configuration.
     */
    void SetConfig(const GeminiConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;
    }
    
    /**
     * Load configuration from the app's Config system.
     */
    void LoadFromConfig() {
        auto& cfg = Config::Instance();
        std::lock_guard<std::mutex> lock(m_mutex);
        
        wxLogDebug("AI: Loading configuration from config system");
        
        // Load provider settings
        std::string providerStr = cfg.GetString("ai.provider", "gemini").ToStdString();
        m_config.provider = GeminiConfig::parseProvider(providerStr);
        m_config.baseUrl = cfg.GetString("ai.baseUrl", "").ToStdString();
        m_config.apiKey = cfg.GetString("ai.apiKey", "").ToStdString();
        
        wxLogDebug("AI: Provider=%s, BaseUrl=%s, ApiKey=%s",
                   providerStr, m_config.baseUrl,
                   m_config.apiKey.empty() ? "(not set)" : "(set)");
        
        // Fallback to legacy gemini-specific key if new key not set
        if (m_config.apiKey.empty()) {
            m_config.apiKey = cfg.GetString("ai.gemini.apiKey", "").ToStdString();
        }
        
        // Load model settings (with provider-specific defaults)
        std::string defaultModel = (m_config.provider == AIProvider::Cortex) 
            ? "gpt-4" : "gemini-1.5-flash";
        m_config.model = cfg.GetString("ai.model", wxString(defaultModel)).ToStdString();
        
        // Fallback to legacy key
        if (m_config.model == defaultModel) {
            std::string legacyModel = cfg.GetString("ai.gemini.model", "").ToStdString();
            if (!legacyModel.empty()) m_config.model = legacyModel;
        }
        
        m_config.temperature = static_cast<float>(cfg.GetDouble("ai.temperature", 0.7));
        m_config.maxOutputTokens = cfg.GetInt("ai.maxOutputTokens", 2048);
        m_config.systemInstruction = cfg.GetString("ai.systemInstruction", "").ToStdString();
    }
    
    /**
     * Save current configuration to the app's Config system.
     */
    void SaveToConfig() {
        auto& cfg = Config::Instance();
        std::lock_guard<std::mutex> lock(m_mutex);
        
        cfg.Set("ai.provider", wxString(m_config.providerName()));
        cfg.Set("ai.baseUrl", wxString(m_config.baseUrl));
        cfg.Set("ai.apiKey", wxString(m_config.apiKey));
        cfg.Set("ai.model", wxString(m_config.model));
        cfg.Set("ai.temperature", static_cast<double>(m_config.temperature));
        cfg.Set("ai.maxOutputTokens", m_config.maxOutputTokens);
        cfg.Set("ai.systemInstruction", wxString(m_config.systemInstruction));
        cfg.Save();
    }
    
    // ========== Single-turn Generation ==========
    
    /**
     * Generate a response for a single prompt.
     * 
     * @param prompt The input prompt
     * @return GeminiResponse with the generated text
     */
    GeminiResponse Generate(const std::string& prompt) {
        std::vector<ChatMessage> messages;
        messages.emplace_back(MessageRole::User, prompt);
        return GenerateFromMessages(messages);
    }
    
    // ========== Conversation Management ==========
    
    /**
     * Start a new conversation, clearing any existing history.
     */
    void StartConversation() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_conversationHistory.clear();
    }
    
    /**
     * Clear the conversation history.
     */
    void ClearConversation() {
        StartConversation();
    }
    
    /**
     * Get the current conversation history.
     */
    std::vector<ChatMessage> GetConversationHistory() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_conversationHistory;
    }
    
    /**
     * Add a message to the conversation history.
     */
    void AddMessage(MessageRole role, const std::string& content) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_conversationHistory.emplace_back(role, content);
    }
    
    /**
     * Enable or disable MCP tool calling.
     */
    void SetMCPEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.enableMCP = enabled;
    }
    
    /**
     * Check if MCP tool calling is enabled.
     */
    bool IsMCPEnabled() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config.enableMCP;
    }
    
    /**
     * Send a message in the current conversation and get a response.
     * The message and response are added to the conversation history.
     * 
     * @param message The user's message
     * @return GeminiResponse with the model's reply
     */
    GeminiResponse SendMessage(const std::string& message) {
        // Add user message to history
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_conversationHistory.emplace_back(MessageRole::User, message);
        }
        
        // Generate response
        std::vector<ChatMessage> messages;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            messages = m_conversationHistory;
        }
        
        GeminiResponse response = GenerateFromMessages(messages);
        
        // Add model response to history if successful
        if (response.isOk()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (response.hasFunctionCall) {
                // For function calls, add a message indicating the model requested a tool
                // This is important for conversation continuity when ContinueWithToolResult is called
                std::string funcCallMsg = "[Calling tool: " + response.functionName + "]";
                if (!response.functionArgs.empty()) {
                    funcCallMsg += "\nArguments: " + response.functionArgs;
                }
                m_conversationHistory.emplace_back(MessageRole::Model, funcCallMsg);
            } else {
                m_conversationHistory.emplace_back(MessageRole::Model, response.text);
            }
        } else {
            // Remove the failed user message
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_conversationHistory.empty()) {
                m_conversationHistory.pop_back();
            }
        }
        // Note: For function calls, the caller is responsible for handling the tool
        // response and continuing the conversation via ContinueWithToolResult
        
        return response;
    }
    
    /**
     * Continue a conversation after a function call with the tool result.
     * This is used when the AI requests a tool call - execute the tool,
     * then call this method with the result to get the AI's final response.
     * 
     * @param functionName The name of the function that was called
     * @param result The result from the tool execution (as JSON string)
     * @return GeminiResponse with the model's continued reply
     */
    GeminiResponse ContinueWithToolResult(const std::string& functionName, 
                                          const std::string& result) {
        // Build the function response message
        // In the conversation history, we need to add:
        // 1. The model's function call (as a model message)
        // 2. The function response (as a special format)
        
        std::string toolResponseContent = "[Tool Result for " + functionName + "]\n" + result;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Add the tool result as a user message (this is how Gemini expects it)
            m_conversationHistory.emplace_back(MessageRole::User, toolResponseContent);
        }
        
        // Generate continuation
        std::vector<ChatMessage> messages;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            messages = m_conversationHistory;
        }
        
        // For the continuation, we might not need tools again
        GeminiResponse response = GenerateFromMessages(messages);
        
        if (response.isOk() && !response.hasFunctionCall) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_conversationHistory.emplace_back(MessageRole::Model, response.text);
        }
        
        return response;
    }
    
    // ========== Available Models ==========
    
    /**
     * Fetch available models from the API for the current provider.
     * Returns cached fallback list if API call fails.
     */
    std::vector<std::string> FetchAvailableModels() {
        GeminiConfig config;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            config = m_config;
        }
        
        wxLogDebug("AI: FetchAvailableModels() for provider %s", config.providerName());
        
        // Need API key to fetch models
        if (config.apiKey.empty()) {
            wxLogWarning("AI: Cannot fetch models - API key not configured");
            return GetFallbackModels(config.provider);
        }
        
        // For Cortex, also need base URL
        if (config.provider == AIProvider::Cortex && config.baseUrl.empty()) {
            wxLogWarning("AI: Cannot fetch Cortex models - base URL not configured");
            return GetFallbackModels(config.provider);
        }
        
        Http::HttpClient& httpClient = Http::getHttpClient();
        if (!httpClient.isAvailable()) {
            wxLogError("AI: HTTP client not available (backend: %s)", httpClient.backendName());
            return GetFallbackModels(config.provider);
        }
        
        Http::HttpRequest request;
        request.method = "GET";
        request.timeoutSeconds = 10;
        
        if (config.provider == AIProvider::Cortex) {
            request.url = config.baseUrl + "/v1/models";
            request.headers["Authorization"] = "Bearer " + config.apiKey;
        } else {
            request.url = config.getEffectiveBaseUrl() + "/models?key=" + config.apiKey;
        }
        
        wxLogDebug("AI: Fetching models from %s", request.url);
        Http::HttpResponse response = httpClient.perform(request);
        
        if (!response.error.empty() || response.statusCode != 200) {
            wxLogError("AI: Failed to fetch models - HTTP %ld, error: %s",
                       response.statusCode, response.error);
            return GetFallbackModels(config.provider);
        }
        
        wxLogDebug("AI: Models response received (HTTP %ld, %zu bytes)",
                   response.statusCode, response.body.size());
        
        // Parse models from response
        std::vector<std::string> models;
        if (config.provider == AIProvider::Cortex) {
            models = ParseCortexModelsResponse(response.body);
        } else {
            models = ParseGeminiModelsResponse(response.body);
        }
        
        // Return fallback if parsing failed
        if (models.empty()) {
            return GetFallbackModels(config.provider);
        }
        
        return models;
    }
    
    /**
     * Get fallback/default models for a provider (used when API is unavailable).
     */
    static std::vector<std::string> GetFallbackModels(AIProvider provider) {
        switch (provider) {
            case AIProvider::Gemini:
                return {
                    "gemini-2.5-flash",        // Stable performance model
                    "gemini-2.5-pro",          // Stable thinking model
                    "gemini-2.0-flash"         // Previous gen flash
                };
            case AIProvider::Cortex:
                return {
                    "gpt-4",           // Most capable
                    "gpt-4-turbo",     // Faster GPT-4
                    "gpt-3.5-turbo"    // Fast and economical
                };
            default:
                return {};
        }
    }
    
private:
    GeminiClient() {
        LoadFromConfig();
    }
    
    ~GeminiClient() = default;
    
    mutable std::mutex m_mutex;
    GeminiConfig m_config;
    std::vector<ChatMessage> m_conversationHistory;
    
    /**
     * Parse Gemini models list response.
     * Response format: { "models": [{ "name": "models/gemini-...", ... }, ...] }
     */
    std::vector<std::string> ParseGeminiModelsResponse(const std::string& body) const {
        std::vector<std::string> models;
        
        // Find all model names - looking for "name": "models/..."
        size_t pos = 0;
        while ((pos = body.find("\"name\"", pos)) != std::string::npos) {
            size_t colonPos = body.find(":", pos);
            if (colonPos == std::string::npos) break;
            
            size_t quoteStart = body.find("\"", colonPos + 1);
            if (quoteStart == std::string::npos) break;
            
            size_t quoteEnd = body.find("\"", quoteStart + 1);
            if (quoteEnd == std::string::npos) break;
            
            std::string fullName = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            
            // Extract model name from "models/gemini-xxx" format
            size_t slashPos = fullName.find("/");
            std::string modelName = (slashPos != std::string::npos) 
                ? fullName.substr(slashPos + 1) 
                : fullName;
            
            // Filter for generative models (skip embedding models, etc.)
            if (modelName.find("gemini") != std::string::npos && 
                modelName.find("embedding") == std::string::npos) {
                models.push_back(modelName);
            }
            
            pos = quoteEnd + 1;
        }
        
        return models;
    }
    
    /**
     * Parse Cortex/OpenAI models list response.
     * Response format: { "data": [{ "id": "gpt-4", ... }, ...] }
     */
    std::vector<std::string> ParseCortexModelsResponse(const std::string& body) const {
        std::vector<std::string> models;
        
        // Find the "data" array
        size_t dataPos = body.find("\"data\"");
        if (dataPos == std::string::npos) return models;
        
        // Find all model IDs within the data array
        size_t pos = dataPos;
        while ((pos = body.find("\"id\"", pos)) != std::string::npos) {
            size_t colonPos = body.find(":", pos);
            if (colonPos == std::string::npos) break;
            
            size_t quoteStart = body.find("\"", colonPos + 1);
            if (quoteStart == std::string::npos) break;
            
            size_t quoteEnd = body.find("\"", quoteStart + 1);
            if (quoteEnd == std::string::npos) break;
            
            std::string modelId = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            
            // Add all models (Cortex may expose various models)
            if (!modelId.empty()) {
                models.push_back(modelId);
            }
            
            pos = quoteEnd + 1;
        }
        
        return models;
    }
    
    /**
     * Build the API endpoint URL.
     */
    std::string BuildEndpoint() const {
        return "https://generativelanguage.googleapis.com/v1beta/models/" 
               + m_config.model + ":generateContent?key=" + m_config.apiKey;
    }
    
    /**
     * Escape a string for JSON.
     */
    std::string EscapeJson(const std::string& str) const {
        std::string result;
        result.reserve(str.size() + 32);
        
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
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
    
    /**
     * Build the request JSON body for the Gemini API.
     */
    std::string BuildRequestBody(const std::vector<ChatMessage>& messages) const {
        return BuildRequestBodyWithTools(messages, m_config.enableMCP);
    }
    
    /**
     * Build the request JSON body for the Gemini API with optional tools.
     */
    std::string BuildRequestBodyWithTools(const std::vector<ChatMessage>& messages, bool includeTools) const {
        std::string json = "{";
        
        // Contents array
        json += "\"contents\":[";
        bool first = true;
        for (const auto& msg : messages) {
            if (!first) json += ",";
            first = false;
            
            json += "{\"role\":\"" + msg.roleString() + "\",";
            json += "\"parts\":[{\"text\":\"" + EscapeJson(msg.content) + "\"}]}";
        }
        json += "]";
        
        // System instruction (if set)
        if (!m_config.systemInstruction.empty()) {
            json += ",\"systemInstruction\":{\"parts\":[{\"text\":\"" 
                 + EscapeJson(m_config.systemInstruction) + "\"}]}";
        }
        
        // Add MCP tools if enabled
        if (includeTools) {
            std::string toolsJson = MCP::Registry::Instance().buildGeminiToolsJson();
            if (!toolsJson.empty()) {
                json += "," + toolsJson;
            }
        }
        
        // Generation config
        json += ",\"generationConfig\":{";
        json += "\"temperature\":" + std::to_string(m_config.temperature) + ",";
        json += "\"maxOutputTokens\":" + std::to_string(m_config.maxOutputTokens) + ",";
        json += "\"topP\":" + std::to_string(m_config.topP) + ",";
        json += "\"topK\":" + std::to_string(m_config.topK);
        json += "}";
        
        // Safety settings
        json += ",\"safetySettings\":[";
        json += "{\"category\":\"HARM_CATEGORY_HARASSMENT\",\"threshold\":\"" + m_config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_HATE_SPEECH\",\"threshold\":\"" + m_config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_SEXUALLY_EXPLICIT\",\"threshold\":\"" + m_config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_DANGEROUS_CONTENT\",\"threshold\":\"" + m_config.safetyThreshold + "\"}";
        json += "]";
        
        json += "}";
        return json;
    }
    
    /**
     * Build the request JSON body for Cortex/OpenAI-compatible API.
     */
    std::string BuildCortexRequestBody(const std::vector<ChatMessage>& messages, 
                                       const GeminiConfig& config) const {
        std::string json = "{";
        
        // Model
        json += "\"model\":\"" + config.model + "\",";
        
        // Messages array (OpenAI format)
        json += "\"messages\":[";
        bool first = true;
        
        // Add system instruction as first message if set
        if (!config.systemInstruction.empty()) {
            json += "{\"role\":\"system\",\"content\":\"" + EscapeJson(config.systemInstruction) + "\"}";
            first = false;
        }
        
        // Add conversation messages
        for (const auto& msg : messages) {
            if (!first) json += ",";
            first = false;
            
            std::string role;
            switch (msg.role) {
                case MessageRole::User: role = "user"; break;
                case MessageRole::Model: role = "assistant"; break;
                case MessageRole::System: role = "system"; break;
            }
            
            json += "{\"role\":\"" + role + "\",\"content\":\"" + EscapeJson(msg.content) + "\"}";
        }
        json += "],";
        
        // Generation parameters
        json += "\"temperature\":" + std::to_string(config.temperature) + ",";
        json += "\"max_tokens\":" + std::to_string(config.maxOutputTokens);
        
        // Optional: add tools if MCP is enabled (OpenAI function calling format)
        // For now, we skip MCP tools for Cortex as the format differs significantly
        
        json += "}";
        return json;
    }
    
    /**
     * Parse the response JSON from Cortex/OpenAI-compatible API.
     */
    GeminiResponse ParseCortexResponse(const std::string& responseBody, long httpCode) const {
        GeminiResponse result;
        result.httpCode = httpCode;
        
        // Check for error response
        size_t errorPos = responseBody.find("\"error\"");
        if (errorPos != std::string::npos) {
            size_t msgStart = responseBody.find("\"message\"", errorPos);
            if (msgStart != std::string::npos) {
                msgStart = responseBody.find("\"", msgStart + 9) + 1;
                size_t msgEnd = responseBody.find("\"", msgStart);
                if (msgEnd != std::string::npos) {
                    result.error = responseBody.substr(msgStart, msgEnd - msgStart);
                }
            }
            if (result.error.empty()) {
                result.error = "API returned an error (HTTP " + std::to_string(httpCode) + ")";
            }
            return result;
        }
        
        // OpenAI format: choices[0].message.content
        size_t choicesPos = responseBody.find("\"choices\"");
        if (choicesPos == std::string::npos) {
            result.error = "Invalid response format: no choices found";
            return result;
        }
        
        // Find message content
        size_t messagePos = responseBody.find("\"message\"", choicesPos);
        if (messagePos == std::string::npos) {
            result.error = "Invalid response format: no message found";
            return result;
        }
        
        size_t contentPos = responseBody.find("\"content\"", messagePos);
        if (contentPos == std::string::npos) {
            result.error = "Invalid response format: no content found";
            return result;
        }
        
        // Extract content value
        size_t textStart = responseBody.find("\"", contentPos + 9) + 1;
        size_t textEnd = textStart;
        
        // Handle escaped quotes in the text
        while (textEnd < responseBody.size()) {
            if (responseBody[textEnd] == '\\') {
                textEnd += 2; // Skip escaped character
                continue;
            }
            if (responseBody[textEnd] == '"') {
                break;
            }
            textEnd++;
        }
        
        std::string text = responseBody.substr(textStart, textEnd - textStart);
        result.text = UnescapeJson(text);
        result.success = true;
        
        // Try to extract token usage
        size_t usagePos = responseBody.find("\"usage\"");
        if (usagePos != std::string::npos) {
            size_t promptPos = responseBody.find("\"prompt_tokens\"", usagePos);
            if (promptPos != std::string::npos) {
                size_t numStart = responseBody.find(":", promptPos) + 1;
                result.promptTokens = std::atoi(responseBody.c_str() + numStart);
            }
            
            size_t completionPos = responseBody.find("\"completion_tokens\"", usagePos);
            if (completionPos != std::string::npos) {
                size_t numStart = responseBody.find(":", completionPos) + 1;
                result.completionTokens = std::atoi(responseBody.c_str() + numStart);
            }
        }
        
        return result;
    }
    
    /**
     * Parse the response JSON from the Gemini API.
     */
    GeminiResponse ParseResponse(const std::string& responseBody, long httpCode) const {
        GeminiResponse result;
        result.httpCode = httpCode;
        
        // Very basic JSON parsing (in production, use a proper JSON library)
        // Look for the text field in the response
        
        // Check for error response
        size_t errorPos = responseBody.find("\"error\"");
        if (errorPos != std::string::npos) {
            size_t msgStart = responseBody.find("\"message\"", errorPos);
            if (msgStart != std::string::npos) {
                msgStart = responseBody.find("\"", msgStart + 9) + 1;
                size_t msgEnd = responseBody.find("\"", msgStart);
                if (msgEnd != std::string::npos) {
                    result.error = responseBody.substr(msgStart, msgEnd - msgStart);
                }
            }
            if (result.error.empty()) {
                result.error = "API returned an error (HTTP " + std::to_string(httpCode) + ")";
            }
            return result;
        }
        
        // Check for function call response
        size_t functionCallPos = responseBody.find("\"functionCall\"");
        if (functionCallPos != std::string::npos) {
            result.hasFunctionCall = true;
            result.success = true;
            
            // Extract function name
            size_t namePos = responseBody.find("\"name\"", functionCallPos);
            if (namePos != std::string::npos) {
                namePos = responseBody.find("\"", namePos + 6) + 1;
                size_t nameEnd = responseBody.find("\"", namePos);
                if (nameEnd != std::string::npos) {
                    result.functionName = responseBody.substr(namePos, nameEnd - namePos);
                }
            }
            
            // Extract function arguments (as JSON object)
            size_t argsPos = responseBody.find("\"args\"", functionCallPos);
            if (argsPos != std::string::npos) {
                // Find the opening brace of the args object
                size_t braceStart = responseBody.find("{", argsPos);
                if (braceStart != std::string::npos) {
                    // Find matching closing brace
                    int braceCount = 1;
                    size_t braceEnd = braceStart + 1;
                    while (braceEnd < responseBody.size() && braceCount > 0) {
                        if (responseBody[braceEnd] == '{') braceCount++;
                        else if (responseBody[braceEnd] == '}') braceCount--;
                        braceEnd++;
                    }
                    result.functionArgs = responseBody.substr(braceStart, braceEnd - braceStart);
                }
            }
            
            return result;
        }
        
        // Find the text content in candidates[0].content.parts[0].text
        size_t textStart = responseBody.find("\"text\"");
        if (textStart == std::string::npos) {
            // Check for blocked content
            if (responseBody.find("BLOCKED") != std::string::npos ||
                responseBody.find("blockReason") != std::string::npos) {
                result.error = "Response was blocked by safety filters";
            } else {
                result.error = "No text found in response";
            }
            return result;
        }
        
        // Extract text value
        textStart = responseBody.find("\"", textStart + 6) + 1;
        size_t textEnd = textStart;
        
        // Handle escaped quotes in the text
        while (textEnd < responseBody.size()) {
            if (responseBody[textEnd] == '\\') {
                textEnd += 2; // Skip escaped character
                continue;
            }
            if (responseBody[textEnd] == '"') {
                break;
            }
            textEnd++;
        }
        
        std::string text = responseBody.substr(textStart, textEnd - textStart);
        
        // Unescape the text
        result.text = UnescapeJson(text);
        result.success = true;
        
        // Try to extract token counts
        size_t tokenPos = responseBody.find("\"promptTokenCount\"");
        if (tokenPos != std::string::npos) {
            size_t numStart = responseBody.find(":", tokenPos) + 1;
            result.promptTokens = std::atoi(responseBody.c_str() + numStart);
        }
        
        tokenPos = responseBody.find("\"candidatesTokenCount\"");
        if (tokenPos != std::string::npos) {
            size_t numStart = responseBody.find(":", tokenPos) + 1;
            result.completionTokens = std::atoi(responseBody.c_str() + numStart);
        }
        
        return result;
    }
    
    /**
     * Unescape JSON string.
     */
    std::string UnescapeJson(const std::string& str) const {
        std::string result;
        result.reserve(str.size());
        
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                switch (str[i + 1]) {
                    case '"':  result += '"'; ++i; break;
                    case '\\': result += '\\'; ++i; break;
                    case 'n':  result += '\n'; ++i; break;
                    case 'r':  result += '\r'; ++i; break;
                    case 't':  result += '\t'; ++i; break;
                    case 'b':  result += '\b'; ++i; break;
                    case 'f':  result += '\f'; ++i; break;
                    case 'u':
                        if (i + 5 < str.size()) {
                            // Parse \uXXXX
                            std::string hex = str.substr(i + 2, 4);
                            int codepoint = std::stoi(hex, nullptr, 16);
                            if (codepoint < 0x80) {
                                result += static_cast<char>(codepoint);
                            } else if (codepoint < 0x800) {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                            i += 5;
                        }
                        break;
                    default:
                        result += str[i + 1];
                        ++i;
                }
            } else {
                result += str[i];
            }
        }
        return result;
    }
    
    /**
     * Generate a response from a list of messages.
     */
    GeminiResponse GenerateFromMessages(const std::vector<ChatMessage>& messages) {
        GeminiResponse result;
        
        // Get config snapshot
        GeminiConfig config;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            config = m_config;
        }
        
        wxLogDebug("AI: GenerateFromMessages() with %zu messages, provider=%s, model=%s",
                   messages.size(), config.providerName(), config.model);
        
        // Validate API key
        if (config.apiKey.empty()) {
            result.error = "API key not configured. Set ai.apiKey in config.";
            wxLogError("AI: %s", result.error);
            return result;
        }
        
        // Validate base URL for Cortex
        if (config.provider == AIProvider::Cortex && config.baseUrl.empty()) {
            result.error = "Base URL not configured. Set ai.baseUrl in config for Cortex.";
            wxLogError("AI: %s", result.error);
            return result;
        }
        
        // Get HTTP client
        Http::HttpClient& httpClient = Http::getHttpClient();
        if (!httpClient.isAvailable()) {
            result.error = "HTTP client not available";
            wxLogError("AI: HTTP client not available (backend: %s)", httpClient.backendName());
            return result;
        }
        
        wxLogDebug("AI: Using HTTP backend: %s", httpClient.backendName());
        
        // Build request based on provider
        Http::HttpRequest request;
        request.method = "POST";
        request.headers["Content-Type"] = "application/json";
        request.timeoutSeconds = 60; // AI requests can take a while
        
        if (config.provider == AIProvider::Cortex) {
            // Cortex/OpenAI-compatible API
            request.url = config.baseUrl + "/v1/chat/completions";
            request.headers["Authorization"] = "Bearer " + config.apiKey;
            request.body = BuildCortexRequestBody(messages, config);
            wxLogDebug("AI: Cortex request to %s", request.url);
        } else {
            // Google Gemini API
            std::string baseUrl = config.getEffectiveBaseUrl();
            request.url = baseUrl + "/models/" + config.model + ":generateContent?key=" + config.apiKey;
            request.body = BuildRequestBody(messages);
            wxLogDebug("AI: Gemini request to %s/models/%s:generateContent", baseUrl, config.model);
        }
        
        wxLogDebug("AI: Request body size: %zu bytes", request.body.size());
        
        Http::HttpResponse httpResponse = httpClient.perform(request);
        
        wxLogDebug("AI: HTTP response - status=%ld, body=%zu bytes, error=%s",
                   httpResponse.statusCode, httpResponse.body.size(),
                   httpResponse.error.empty() ? "(none)" : httpResponse.error.c_str());
        
        if (!httpResponse.error.empty()) {
            result.error = httpResponse.error;
            result.httpCode = httpResponse.statusCode;
            wxLogError("AI: Request failed - HTTP %ld: %s", result.httpCode, result.error);
            // Log response body for debugging (truncated)
            if (!httpResponse.body.empty()) {
                std::string bodyPreview = httpResponse.body.substr(0, 500);
                wxLogDebug("AI: Response body preview: %s%s", bodyPreview,
                           httpResponse.body.size() > 500 ? "..." : "");
            }
            return result;
        }
        
        // Parse response based on provider
        GeminiResponse parsedResult;
        if (config.provider == AIProvider::Cortex) {
            parsedResult = ParseCortexResponse(httpResponse.body, httpResponse.statusCode);
        } else {
            parsedResult = ParseResponse(httpResponse.body, httpResponse.statusCode);
        }
        
        if (!parsedResult.success) {
            wxLogError("AI: Failed to parse response - %s", parsedResult.error);
            // Log response body for debugging (truncated)
            std::string bodyPreview = httpResponse.body.substr(0, 500);
            wxLogDebug("AI: Response body preview: %s%s", bodyPreview,
                       httpResponse.body.size() > 500 ? "..." : "");
        } else {
            wxLogDebug("AI: Response parsed successfully - tokens: prompt=%d, completion=%d",
                       parsedResult.promptTokens, parsedResult.completionTokens);
            if (parsedResult.hasFunctionCall) {
                wxLogDebug("AI: Function call requested: %s", parsedResult.functionName);
            }
        }
        
        return parsedResult;
    }
};

} // namespace AI

#endif // GEMINI_CLIENT_H
