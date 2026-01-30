#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include "ai_types.h"
#include "ai_provider_gemini.h"
#include "ai_provider_cortex.h"
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

// Types are imported from ai_types.h
// ChatMessage, AIResponse (GeminiResponse), AIConfig (GeminiConfig), AIProvider, MessageRole, StreamCallback

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
        
        // Safety settings - for dev tools, default to less restrictive
        // Options: BLOCK_NONE, BLOCK_ONLY_HIGH, BLOCK_MEDIUM_AND_ABOVE, BLOCK_LOW_AND_ABOVE
        m_config.safetyThreshold = cfg.GetString("ai.safetyThreshold", "BLOCK_ONLY_HIGH").ToStdString();
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
     * Parse Gemini models list response - delegates to GeminiProvider.
     */
    std::vector<std::string> ParseGeminiModelsResponse(const std::string& body) const {
        return GeminiProvider::parseModelsResponse(body);
    }
    
    /**
     * Parse Cortex/OpenAI models list response - delegates to CortexProvider.
     */
    std::vector<std::string> ParseCortexModelsResponse(const std::string& body) const {
        return CortexProvider::parseModelsResponse(body);
    }
    
    /**
     * Build the API endpoint URL.
     */
    std::string BuildEndpoint() const {
        return "https://generativelanguage.googleapis.com/v1beta/models/" 
               + m_config.model + ":generateContent?key=" + m_config.apiKey;
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
            json += "\"parts\":[{\"text\":\"" + escapeJson(msg.content) + "\"}]}";
        }
        json += "]";
        
        // System instruction (if set)
        if (!m_config.systemInstruction.empty()) {
            json += ",\"systemInstruction\":{\"parts\":[{\"text\":\"" 
                 + escapeJson(m_config.systemInstruction) + "\"}]}";
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
            json += "{\"role\":\"system\",\"content\":\"" + escapeJson(config.systemInstruction) + "\"}";
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
            
            json += "{\"role\":\"" + role + "\",\"content\":\"" + escapeJson(msg.content) + "\"}";
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
     * Delegates to CortexProvider which uses Glaze for proper JSON parsing.
     */
    GeminiResponse ParseCortexResponse(const std::string& responseBody, long httpCode) const {
        return CortexProvider::parseResponse(responseBody, httpCode);
    }
    
    /**
     * Parse the response JSON from the Gemini API.
     * Delegates to GeminiProvider which uses Glaze for proper JSON parsing.
     */
    GeminiResponse ParseResponse(const std::string& responseBody, long httpCode) const {
        return GeminiProvider::parseResponse(responseBody, httpCode);
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
