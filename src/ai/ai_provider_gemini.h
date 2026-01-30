#ifndef AI_PROVIDER_GEMINI_H
#define AI_PROVIDER_GEMINI_H

#include "ai_types.h"
#include "../http/http_client.h"
#include "../mcp/mcp.h"
#include <wx/log.h>
#include <string>
#include <vector>

namespace AI {

/**
 * Gemini API response structures for Glaze JSON parsing.
 */
namespace GeminiApi {
    struct FunctionCall {
        std::string name;
        std::map<std::string, glz::generic> args;
    };

    struct Part {
        std::optional<std::string> text;
        std::optional<FunctionCall> functionCall;
    };

    struct Content {
        std::string role;
        std::vector<Part> parts;
    };

    struct SafetyRating {
        std::string category;
        std::string probability;
    };

    struct Candidate {
        Content content;
        std::optional<std::string> finishReason;
        std::vector<SafetyRating> safetyRatings;
    };

    struct UsageMetadata {
        int promptTokenCount = 0;
        int candidatesTokenCount = 0;
        int totalTokenCount = 0;
    };

    struct ErrorDetail {
        std::string message;
        int code = 0;
        std::string status;
    };

    struct Response {
        std::vector<Candidate> candidates;
        std::optional<UsageMetadata> usageMetadata;
        std::optional<ErrorDetail> error;
    };

    struct ModelInfo {
        std::string name;
        std::string displayName;
        std::string description;
    };

    struct ModelsResponse {
        std::vector<ModelInfo> models;
    };
} // namespace GeminiApi

} // namespace AI

// Glaze metadata for Gemini API types
template<> struct glz::meta<AI::GeminiApi::FunctionCall> {
    using T = AI::GeminiApi::FunctionCall;
    static constexpr auto value = object("name", &T::name, "args", &T::args);
};

template<> struct glz::meta<AI::GeminiApi::Part> {
    using T = AI::GeminiApi::Part;
    static constexpr auto value = object("text", &T::text, "functionCall", &T::functionCall);
};

template<> struct glz::meta<AI::GeminiApi::Content> {
    using T = AI::GeminiApi::Content;
    static constexpr auto value = object("role", &T::role, "parts", &T::parts);
};

template<> struct glz::meta<AI::GeminiApi::SafetyRating> {
    using T = AI::GeminiApi::SafetyRating;
    static constexpr auto value = object("category", &T::category, "probability", &T::probability);
};

template<> struct glz::meta<AI::GeminiApi::Candidate> {
    using T = AI::GeminiApi::Candidate;
    static constexpr auto value = object("content", &T::content, "finishReason", &T::finishReason, "safetyRatings", &T::safetyRatings);
};

template<> struct glz::meta<AI::GeminiApi::UsageMetadata> {
    using T = AI::GeminiApi::UsageMetadata;
    static constexpr auto value = object("promptTokenCount", &T::promptTokenCount, "candidatesTokenCount", &T::candidatesTokenCount, "totalTokenCount", &T::totalTokenCount);
};

template<> struct glz::meta<AI::GeminiApi::ErrorDetail> {
    using T = AI::GeminiApi::ErrorDetail;
    static constexpr auto value = object("message", &T::message, "code", &T::code, "status", &T::status);
};

template<> struct glz::meta<AI::GeminiApi::Response> {
    using T = AI::GeminiApi::Response;
    static constexpr auto value = object("candidates", &T::candidates, "usageMetadata", &T::usageMetadata, "error", &T::error);
};

template<> struct glz::meta<AI::GeminiApi::ModelInfo> {
    using T = AI::GeminiApi::ModelInfo;
    static constexpr auto value = object("name", &T::name, "displayName", &T::displayName, "description", &T::description);
};

template<> struct glz::meta<AI::GeminiApi::ModelsResponse> {
    using T = AI::GeminiApi::ModelsResponse;
    static constexpr auto value = object("models", &T::models);
};

namespace AI {

/**
 * Google Gemini API provider backend.
 * Implements compile-time polymorphic interface for AI providers.
 */
class GeminiProvider {
public:
    static constexpr const char* name() { return "gemini"; }
    
    static std::string getDefaultBaseUrl() {
        return "https://generativelanguage.googleapis.com/v1beta";
    }
    
    static std::vector<std::string> getFallbackModels() {
        return {
            "gemini-2.5-flash",
            "gemini-2.5-pro",
            "gemini-2.0-flash"
        };
    }
    
    /**
     * Build the request URL for generate content.
     */
    static std::string buildRequestUrl(const AIConfig& config) {
        std::string baseUrl = config.baseUrl.empty() ? getDefaultBaseUrl() : config.baseUrl;
        return baseUrl + "/models/" + config.model + ":generateContent?key=" + config.apiKey;
    }
    
    /**
     * Build request headers.
     */
    static std::map<std::string, std::string> buildRequestHeaders(const AIConfig& /*config*/) {
        return {{"Content-Type", "application/json"}};
    }
    
    /**
     * Build the request body JSON.
     */
    static std::string buildRequestBody(const std::vector<ChatMessage>& messages, 
                                        const AIConfig& config,
                                        bool includeTools = true) {
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
        
        // System instruction
        if (!config.systemInstruction.empty()) {
            json += ",\"systemInstruction\":{\"parts\":[{\"text\":\"" 
                 + escapeJson(config.systemInstruction) + "\"}]}";
        }
        
        // MCP tools
        if (includeTools && config.enableMCP) {
            std::string toolsJson = MCP::Registry::Instance().buildGeminiToolsJson();
            if (!toolsJson.empty()) {
                json += "," + toolsJson;
            }
        }
        
        // Generation config
        json += ",\"generationConfig\":{";
        json += "\"temperature\":" + std::to_string(config.temperature) + ",";
        json += "\"maxOutputTokens\":" + std::to_string(config.maxOutputTokens) + ",";
        json += "\"topP\":" + std::to_string(config.topP) + ",";
        json += "\"topK\":" + std::to_string(config.topK);
        json += "}";
        
        // Safety settings
        json += ",\"safetySettings\":[";
        json += "{\"category\":\"HARM_CATEGORY_HARASSMENT\",\"threshold\":\"" + config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_HATE_SPEECH\",\"threshold\":\"" + config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_SEXUALLY_EXPLICIT\",\"threshold\":\"" + config.safetyThreshold + "\"},";
        json += "{\"category\":\"HARM_CATEGORY_DANGEROUS_CONTENT\",\"threshold\":\"" + config.safetyThreshold + "\"}";
        json += "]";
        
        json += "}";
        return json;
    }
    
    /**
     * Parse the API response.
     */
    static AIResponse parseResponse(const std::string& responseBody, long httpCode) {
        AIResponse result;
        result.httpCode = httpCode;
        
        GeminiApi::Response apiResponse;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(apiResponse, responseBody);
        
        if (ec) {
            result.error = "Failed to parse JSON response: " + glz::format_error(ec, responseBody);
            wxLogError("AI: %s", result.error);
            return result;
        }
        
        // Check for error
        if (apiResponse.error.has_value()) {
            result.error = apiResponse.error->message;
            if (result.error.empty()) {
                result.error = "API returned an error (HTTP " + std::to_string(httpCode) + ")";
            }
            return result;
        }
        
        // Check for candidates
        if (apiResponse.candidates.empty()) {
            result.error = "No candidates in response";
            return result;
        }
        
        const auto& candidate = apiResponse.candidates[0];
        
        // Check for safety block
        if (candidate.finishReason.has_value() && candidate.finishReason.value() == "SAFETY") {
            std::string category = "unknown";
            for (const auto& rating : candidate.safetyRatings) {
                if (rating.probability == "MEDIUM" || rating.probability == "HIGH") {
                    category = rating.category;
                    break;
                }
            }
            result.error = "Response blocked by safety filter (" + category + "). "
                          "Try setting ai.safetyThreshold to BLOCK_ONLY_HIGH or BLOCK_NONE in config.";
            return result;
        }
        
        // Extract content
        for (const auto& part : candidate.content.parts) {
            if (part.functionCall.has_value()) {
                result.hasFunctionCall = true;
                result.success = true;
                result.functionName = part.functionCall->name;
                result.functionArgs = glz::write_json(part.functionCall->args).value_or("{}");
                return result;
            }
            
            if (part.text.has_value()) {
                result.text = part.text.value();
                result.success = true;
            }
        }
        
        if (!result.success) {
            result.error = "No text or function call found in response";
            return result;
        }
        
        // Token counts
        if (apiResponse.usageMetadata.has_value()) {
            result.promptTokens = apiResponse.usageMetadata->promptTokenCount;
            result.completionTokens = apiResponse.usageMetadata->candidatesTokenCount;
        }
        
        return result;
    }
    
    /**
     * Build URL for fetching available models.
     */
    static std::string buildModelsUrl(const AIConfig& config) {
        std::string baseUrl = config.baseUrl.empty() ? getDefaultBaseUrl() : config.baseUrl;
        return baseUrl + "/models?key=" + config.apiKey;
    }
    
    /**
     * Build headers for models request.
     */
    static std::map<std::string, std::string> buildModelsHeaders(const AIConfig& /*config*/) {
        return {};
    }
    
    /**
     * Parse models list response.
     */
    static std::vector<std::string> parseModelsResponse(const std::string& body) {
        std::vector<std::string> models;
        
        GeminiApi::ModelsResponse modelsResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(modelsResp, body);
        
        if (ec) {
            wxLogError("AI: Failed to parse models response: %s", 
                       glz::format_error(ec, body).c_str());
            return models;
        }
        
        for (const auto& modelInfo : modelsResp.models) {
            std::string modelName = modelInfo.name;
            size_t slashPos = modelName.find("/");
            if (slashPos != std::string::npos) {
                modelName = modelName.substr(slashPos + 1);
            }
            
            if (modelName.find("gemini") != std::string::npos && 
                modelName.find("embedding") == std::string::npos) {
                models.push_back(modelName);
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
        return ""; // Valid
    }
};

} // namespace AI

#endif // AI_PROVIDER_GEMINI_H
