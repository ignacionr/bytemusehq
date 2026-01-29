#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include "../http/http_client.h"
#include "../config/config.h"
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
    
    bool isOk() const { return success && error.empty(); }
};

/**
 * Configuration for Gemini API calls.
 */
struct GeminiConfig {
    std::string apiKey;
    std::string model = "gemini-1.5-flash";
    float temperature = 0.7f;
    int maxOutputTokens = 2048;
    float topP = 0.95f;
    int topK = 40;
    std::string systemInstruction;
    
    // Safety settings - block thresholds
    // Options: BLOCK_NONE, BLOCK_ONLY_HIGH, BLOCK_MEDIUM_AND_ABOVE, BLOCK_LOW_AND_ABOVE
    std::string safetyThreshold = "BLOCK_MEDIUM_AND_ABOVE";
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
        
        m_config.apiKey = cfg.GetString("ai.gemini.apiKey", "").ToStdString();
        m_config.model = cfg.GetString("ai.gemini.model", "gemini-1.5-flash").ToStdString();
        m_config.temperature = static_cast<float>(cfg.GetDouble("ai.gemini.temperature", 0.7));
        m_config.maxOutputTokens = cfg.GetInt("ai.gemini.maxOutputTokens", 2048);
        m_config.systemInstruction = cfg.GetString("ai.gemini.systemInstruction", "").ToStdString();
    }
    
    /**
     * Save current configuration to the app's Config system.
     */
    void SaveToConfig() {
        auto& cfg = Config::Instance();
        std::lock_guard<std::mutex> lock(m_mutex);
        
        cfg.Set("ai.gemini.apiKey", wxString(m_config.apiKey));
        cfg.Set("ai.gemini.model", wxString(m_config.model));
        cfg.Set("ai.gemini.temperature", static_cast<double>(m_config.temperature));
        cfg.Set("ai.gemini.maxOutputTokens", m_config.maxOutputTokens);
        cfg.Set("ai.gemini.systemInstruction", wxString(m_config.systemInstruction));
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
            m_conversationHistory.emplace_back(MessageRole::Model, response.text);
        } else {
            // Remove the failed user message
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_conversationHistory.empty()) {
                m_conversationHistory.pop_back();
            }
        }
        
        return response;
    }
    
    // ========== Available Models ==========
    
    /**
     * Get list of available Gemini models.
     */
    static std::vector<std::string> GetAvailableModels() {
        return {
            "gemini-3-pro-preview",    // Best for complex logic & math
            "gemini-3-flash-preview",  // Best for agentic coding & speed (Recommended)
            "gemini-2.5-pro",          // Stable thinking model
            "gemini-2.5-flash",        // Stable performance model
            "gemini-2.5-flash-lite"    // Most cost-efficient for small tasks
        };
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
        
        // Validate API key
        if (config.apiKey.empty()) {
            result.error = "API key not configured. Set ai.gemini.apiKey in config.";
            return result;
        }
        
        // Get HTTP client
        Http::HttpClient& httpClient = Http::getHttpClient();
        if (!httpClient.isAvailable()) {
            result.error = "HTTP client not available";
            return result;
        }
        
        // Build request
        std::string endpoint = "https://generativelanguage.googleapis.com/v1beta/models/" 
                              + config.model + ":generateContent?key=" + config.apiKey;
        std::string body = BuildRequestBody(messages);
        
        // Make HTTP request
        Http::HttpRequest request;
        request.url = endpoint;
        request.method = "POST";
        request.body = body;
        request.headers["Content-Type"] = "application/json";
        request.timeoutSeconds = 60; // AI requests can take a while
        
        Http::HttpResponse httpResponse = httpClient.perform(request);
        
        if (!httpResponse.error.empty()) {
            result.error = httpResponse.error;
            result.httpCode = httpResponse.statusCode;
            return result;
        }
        
        // Parse and return response
        return ParseResponse(httpResponse.body, httpResponse.statusCode);
    }
};

} // namespace AI

#endif // GEMINI_CLIENT_H
