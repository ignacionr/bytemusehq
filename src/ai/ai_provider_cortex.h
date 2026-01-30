#ifndef AI_PROVIDER_CORTEX_H
#define AI_PROVIDER_CORTEX_H

#include "ai_types.h"
#include "../http/http_client.h"
#include <wx/log.h>
#include <string>
#include <vector>

namespace AI {

/**
 * OpenAI/Cortex API response structures for Glaze JSON parsing.
 */
namespace CortexApi {
    struct Message {
        std::string role;
        std::string content;
    };

    struct Choice {
        int index = 0;
        Message message;
        std::optional<std::string> finish_reason;
    };

    struct Usage {
        int prompt_tokens = 0;
        int completion_tokens = 0;
        int total_tokens = 0;
    };

    struct ErrorDetail {
        std::string message;
        std::string type;
        std::string code;
    };

    struct ErrorWrapper {
        ErrorDetail error;
    };

    struct Response {
        std::string id;
        std::string object;
        std::vector<Choice> choices;
        std::optional<Usage> usage;
    };

    struct ModelInfo {
        std::string id;
        std::string object;
        std::optional<std::string> owned_by;
    };

    struct ModelsResponse {
        std::string object;
        std::vector<ModelInfo> data;
    };
} // namespace CortexApi

} // namespace AI

// Glaze metadata for Cortex/OpenAI API types
template<> struct glz::meta<AI::CortexApi::Message> {
    using T = AI::CortexApi::Message;
    static constexpr auto value = object("role", &T::role, "content", &T::content);
};

template<> struct glz::meta<AI::CortexApi::Choice> {
    using T = AI::CortexApi::Choice;
    static constexpr auto value = object("index", &T::index, "message", &T::message, "finish_reason", &T::finish_reason);
};

template<> struct glz::meta<AI::CortexApi::Usage> {
    using T = AI::CortexApi::Usage;
    static constexpr auto value = object("prompt_tokens", &T::prompt_tokens, "completion_tokens", &T::completion_tokens, "total_tokens", &T::total_tokens);
};

template<> struct glz::meta<AI::CortexApi::ErrorDetail> {
    using T = AI::CortexApi::ErrorDetail;
    static constexpr auto value = object("message", &T::message, "type", &T::type, "code", &T::code);
};

template<> struct glz::meta<AI::CortexApi::ErrorWrapper> {
    using T = AI::CortexApi::ErrorWrapper;
    static constexpr auto value = object("error", &T::error);
};

template<> struct glz::meta<AI::CortexApi::Response> {
    using T = AI::CortexApi::Response;
    static constexpr auto value = object("id", &T::id, "object", &T::object, "choices", &T::choices, "usage", &T::usage);
};

template<> struct glz::meta<AI::CortexApi::ModelInfo> {
    using T = AI::CortexApi::ModelInfo;
    static constexpr auto value = object("id", &T::id, "object", &T::object, "owned_by", &T::owned_by);
};

template<> struct glz::meta<AI::CortexApi::ModelsResponse> {
    using T = AI::CortexApi::ModelsResponse;
    static constexpr auto value = object("object", &T::object, "data", &T::data);
};

namespace AI {

/**
 * Cortex/OpenAI-compatible API provider backend.
 * Implements compile-time polymorphic interface for AI providers.
 */
class CortexProvider {
public:
    static constexpr const char* name() { return "cortex"; }
    
    static std::string getDefaultBaseUrl() {
        return ""; // Must be configured by user
    }
    
    static std::vector<std::string> getFallbackModels() {
        return {
            "gpt-4",
            "gpt-4-turbo",
            "gpt-3.5-turbo"
        };
    }
    
    /**
     * Build the request URL for chat completions.
     */
    static std::string buildRequestUrl(const AIConfig& config) {
        return config.baseUrl + "/v1/chat/completions";
    }
    
    /**
     * Build request headers with Bearer token auth.
     */
    static std::map<std::string, std::string> buildRequestHeaders(const AIConfig& config) {
        return {
            {"Content-Type", "application/json"},
            {"Authorization", "Bearer " + config.apiKey}
        };
    }
    
    /**
     * Build the request body JSON (OpenAI format).
     */
    static std::string buildRequestBody(const std::vector<ChatMessage>& messages, 
                                        const AIConfig& config,
                                        bool /*includeTools*/ = true) {
        std::string json = "{";
        
        // Model
        json += "\"model\":\"" + config.model + "\",";
        
        // Messages array
        json += "\"messages\":[";
        bool first = true;
        
        // System instruction as first message
        if (!config.systemInstruction.empty()) {
            json += "{\"role\":\"system\",\"content\":\"" + escapeJson(config.systemInstruction) + "\"}";
            first = false;
        }
        
        // Conversation messages
        for (const auto& msg : messages) {
            if (!first) json += ",";
            first = false;
            
            json += "{\"role\":\"" + msg.openAiRoleString() + "\",\"content\":\"" + escapeJson(msg.content) + "\"}";
        }
        json += "],";
        
        // Generation parameters
        json += "\"temperature\":" + std::to_string(config.temperature) + ",";
        json += "\"max_tokens\":" + std::to_string(config.maxOutputTokens);
        
        // TODO: Add OpenAI function calling format if MCP enabled
        
        json += "}";
        return json;
    }
    
    /**
     * Parse the API response.
     */
    static AIResponse parseResponse(const std::string& responseBody, long httpCode) {
        AIResponse result;
        result.httpCode = httpCode;
        
        // Try parsing as error first
        CortexApi::ErrorWrapper errResp;
        auto errEc = glz::read<glz::opts{.error_on_unknown_keys = false}>(errResp, responseBody);
        if (!errEc && !errResp.error.message.empty()) {
            result.error = errResp.error.message;
            return result;
        }
        
        // Parse as success response
        CortexApi::Response apiResponse;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(apiResponse, responseBody);
        
        if (ec) {
            result.error = "Failed to parse JSON response: " + glz::format_error(ec, responseBody);
            wxLogError("AI: %s", result.error);
            return result;
        }
        
        // Check for choices
        if (apiResponse.choices.empty()) {
            result.error = "Invalid response format: no choices found";
            return result;
        }
        
        const auto& choice = apiResponse.choices[0];
        
        // Extract content
        result.text = choice.message.content;
        result.success = true;
        
        // Token usage
        if (apiResponse.usage.has_value()) {
            result.promptTokens = apiResponse.usage->prompt_tokens;
            result.completionTokens = apiResponse.usage->completion_tokens;
        }
        
        return result;
    }
    
    /**
     * Build URL for fetching available models.
     */
    static std::string buildModelsUrl(const AIConfig& config) {
        return config.baseUrl + "/v1/models";
    }
    
    /**
     * Build headers for models request.
     */
    static std::map<std::string, std::string> buildModelsHeaders(const AIConfig& config) {
        return {{"Authorization", "Bearer " + config.apiKey}};
    }
    
    /**
     * Parse models list response.
     */
    static std::vector<std::string> parseModelsResponse(const std::string& body) {
        std::vector<std::string> models;
        
        CortexApi::ModelsResponse modelsResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(modelsResp, body);
        
        if (ec) {
            wxLogError("AI: Failed to parse Cortex models response: %s", 
                       glz::format_error(ec, body).c_str());
            return models;
        }
        
        for (const auto& modelInfo : modelsResp.data) {
            if (!modelInfo.id.empty()) {
                models.push_back(modelInfo.id);
            }
        }
        
        return models;
    }
    
    /**
     * Validate configuration for this provider.
     */
    static std::string validateConfig(const AIConfig& config) {
        if (config.apiKey.empty()) {
            return "API key not configured. Set ai.apiKey in config.";
        }
        if (config.baseUrl.empty()) {
            return "Base URL not configured. Set ai.baseUrl in config for Cortex.";
        }
        return ""; // Valid
    }
};

} // namespace AI

#endif // AI_PROVIDER_CORTEX_H
