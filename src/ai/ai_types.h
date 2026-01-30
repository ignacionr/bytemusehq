#ifndef AI_TYPES_H
#define AI_TYPES_H

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <map>
#include <glaze/glaze.hpp>

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
    
    std::string openAiRoleString() const {
        switch (role) {
            case MessageRole::User: return "user";
            case MessageRole::Model: return "assistant";
            case MessageRole::System: return "system";
            default: return "user";
        }
    }
};

/**
 * Result of an AI API call.
 */
struct AIResponse {
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

// Legacy alias for backwards compatibility
using GeminiResponse = AIResponse;

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
struct AIConfig {
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
    std::string safetyThreshold = "BLOCK_ONLY_HIGH";
    
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

// Legacy alias for backwards compatibility
using GeminiConfig = AIConfig;

/**
 * Callback for streaming responses.
 */
using StreamCallback = std::function<bool(const std::string& chunk)>;

/**
 * Escape a string for JSON embedding.
 */
inline std::string escapeJson(const std::string& str) {
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

} // namespace AI

#endif // AI_TYPES_H
