#ifndef JIRA_CLIENT_H
#define JIRA_CLIENT_H

#include "../http/http_client.h"
#include "../config/config.h"

#include <glaze/glaze.hpp>

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

namespace Jira {

/**
 * JIRA API response structures for Glaze JSON parsing.
 * These map to the JIRA REST API v2/v3 response format.
 */
namespace Api {
    struct NamedField {
        std::string name;
    };
    
    struct User {
        std::optional<std::string> displayName;
        std::optional<std::string> accountId;  // API v3
        std::optional<std::string> key;        // API v2 (legacy)
    };
    
    struct IssueFields {
        std::string summary;
        std::optional<std::string> description;
        std::string updated;
        std::optional<NamedField> status;
        std::optional<NamedField> priority;
        std::optional<NamedField> issuetype;
        std::optional<User> assignee;
        std::optional<User> reporter;
    };
    
    struct Issue {
        std::string id;
        std::string key;
        IssueFields fields;
    };
    
    struct SearchResponse {
        int total = 0;
        int maxResults = 0;
        std::vector<Issue> issues;
    };
    
    struct ErrorResponse {
        std::vector<std::string> errorMessages;
        std::string message;  // Single error message (used by some endpoints)
    };
    
    struct CreateIssueResponse {
        std::string id;
        std::string key;
        std::string self;
    };
    
    struct Project {
        std::string id;
        std::string key;
        std::string name;
    };
    
    struct IssueType {
        std::string id;
        std::string name;
        std::optional<std::string> description;
        bool subtask = false;
    };
    
    struct Priority {
        std::string id;
        std::string name;
    };
    
    struct Transition {
        std::string id;
        std::string name;
        NamedField to;
    };
    
    struct TransitionsResponse {
        std::vector<Transition> transitions;
    };
    
    struct Comment {
        std::string id;
        std::string body;
        User author;
        std::string created;
        std::string updated;
    };
    
    struct CommentsResponse {
        int total = 0;
        std::vector<Comment> comments;
    };
} // namespace Api

/**
 * Simplified issue structure for general use.
 */
struct Issue {
    std::string key;           // e.g., "PROJ-123"
    std::string summary;       // Issue title
    std::string description;   // Issue description
    std::string status;        // "To Do", "In Progress", "Done", etc.
    std::string priority;      // "Highest", "High", "Medium", "Low", "Lowest"
    std::string type;          // "Bug", "Story", "Task", "Epic"
    std::string assignee;      // Assignee display name
    std::string reporter;      // Reporter display name
    std::string updated;       // Last updated timestamp
    std::string url;           // Web URL to the issue
    
    /**
     * Create from API issue.
     */
    static Issue FromApi(const Api::Issue& apiIssue, const std::string& baseUrl) {
        Issue issue;
        issue.key = apiIssue.key;
        issue.summary = apiIssue.fields.summary;
        issue.description = apiIssue.fields.description.value_or("");
        
        if (apiIssue.fields.status) {
            issue.status = apiIssue.fields.status->name;
        }
        if (apiIssue.fields.priority) {
            issue.priority = apiIssue.fields.priority->name;
        }
        if (apiIssue.fields.issuetype) {
            issue.type = apiIssue.fields.issuetype->name;
        }
        if (apiIssue.fields.assignee) {
            issue.assignee = apiIssue.fields.assignee->displayName.value_or("Unknown");
        }
        if (apiIssue.fields.reporter) {
            issue.reporter = apiIssue.fields.reporter->displayName.value_or("Unknown");
        }
        
        issue.updated = apiIssue.fields.updated;
        issue.url = baseUrl + "/browse/" + issue.key;
        
        return issue;
    }
};

/**
 * Comment structure for general use.
 */
struct Comment {
    std::string id;
    std::string body;
    std::string author;
    std::string created;
    std::string updated;
    
    static Comment FromApi(const Api::Comment& apiComment) {
        Comment comment;
        comment.id = apiComment.id;
        comment.body = apiComment.body;
        comment.author = apiComment.author.displayName.value_or("Unknown");
        comment.created = apiComment.created;
        comment.updated = apiComment.updated;
        return comment;
    }
};

/**
 * Transition structure.
 */
struct Transition {
    std::string id;
    std::string name;
    std::string toStatus;
    
    static Transition FromApi(const Api::Transition& apiTransition) {
        Transition t;
        t.id = apiTransition.id;
        t.name = apiTransition.name;
        t.toStatus = apiTransition.to.name;
        return t;
    }
};

/**
 * Configuration for the Jira client.
 */
struct ClientConfig {
    std::string apiUrl;          // e.g., "https://mycompany.atlassian.net"
    std::string user;            // email for Cloud, username for Server
    std::string apiToken;        // API token or password
    std::string defaultProject;  // Default project key
    std::string apiVersion = "2"; // "2" for Server, "3" for Cloud
    int timeoutSeconds = 30;
    
    bool isValid() const {
        return !apiUrl.empty() && !user.empty() && !apiToken.empty();
    }
    
    /**
     * Load configuration from Config singleton.
     */
    static ClientConfig LoadFromConfig() {
        auto& config = Config::Instance();
        ClientConfig cfg;
        cfg.apiUrl = config.GetString("jira.apiUrl", "").ToStdString();
        cfg.user = config.GetString("jira.user", "").ToStdString();
        cfg.apiToken = config.GetString("jira.apiToken", "").ToStdString();
        cfg.defaultProject = config.GetString("jira.defaultProject", "").ToStdString();
        cfg.apiVersion = config.GetString("jira.apiVersion", "2").ToStdString();
        return cfg;
    }
};

/**
 * Result of a Jira API operation.
 */
template<typename T>
struct Result {
    bool success = false;
    T data;
    std::string error;
    long httpCode = 0;
    
    static Result<T> Success(T value) {
        Result<T> r;
        r.success = true;
        r.data = std::move(value);
        return r;
    }
    
    static Result<T> Error(const std::string& msg, long code = 0) {
        Result<T> r;
        r.success = false;
        r.error = msg;
        r.httpCode = code;
        return r;
    }
};

/**
 * Jira API client - platform-independent core functionality.
 * Uses the Http::HttpClient for network requests.
 */
class Client {
public:
    Client() = default;
    
    explicit Client(const ClientConfig& config) : m_config(config) {}
    
    /**
     * Set/update the configuration.
     */
    void SetConfig(const ClientConfig& config) {
        m_config = config;
    }
    
    /**
     * Get current configuration.
     */
    const ClientConfig& GetConfig() const {
        return m_config;
    }
    
    /**
     * Check if the client is configured.
     */
    bool IsConfigured() const {
        return m_config.isValid();
    }
    
    /**
     * Search for issues using JQL.
     * @param jql The JQL query string
     * @param maxResults Maximum number of results (default 50)
     * @param fields Fields to retrieve (empty for default)
     */
    Result<std::vector<Issue>> SearchIssues(
        const std::string& jql,
        int maxResults = 50,
        const std::vector<std::string>& fields = {}
    ) {
        if (!IsConfigured()) {
            return Result<std::vector<Issue>>::Error("Jira client not configured");
        }
        
        auto apiResult = SearchIssuesRaw(jql, maxResults, fields);
        if (!apiResult.success) {
            return Result<std::vector<Issue>>::Error(apiResult.error, apiResult.httpCode);
        }
        
        std::vector<Issue> issues;
        for (const auto& apiIssue : apiResult.data.issues) {
            issues.push_back(Issue::FromApi(apiIssue, m_config.apiUrl));
        }
        
        return Result<std::vector<Issue>>::Success(std::move(issues));
    }
    
    /**
     * Search for issues (raw API response).
     */
    Result<Api::SearchResponse> SearchIssuesRaw(
        const std::string& jql,
        int maxResults = 50,
        const std::vector<std::string>& fields = {}
    ) {
        if (!IsConfigured()) {
            return Result<Api::SearchResponse>::Error("Jira client not configured");
        }
        
        // Build fields list
        std::string fieldsParam = "key,summary,description,status,priority,issuetype,assignee,reporter,updated";
        if (!fields.empty()) {
            fieldsParam = "";
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) fieldsParam += ",";
                fieldsParam += fields[i];
            }
        }
        
        std::string response;
        long httpCode = 0;
        std::string error;
        
        if (m_config.apiVersion == "3") {
            // API v3 uses POST with JSON body
            std::string jsonBody = R"({"jql": ")" + EscapeJson(jql) + R"(", "fields": [")" + 
                fieldsParam + R"("], "maxResults": )" + std::to_string(maxResults) + "}";
            // Fix fields format - split by comma and quote each
            jsonBody = "{\"jql\": \"" + EscapeJson(jql) + "\", \"fields\": [";
            bool first = true;
            size_t start = 0;
            while (start < fieldsParam.size()) {
                size_t end = fieldsParam.find(',', start);
                if (end == std::string::npos) end = fieldsParam.size();
                if (!first) jsonBody += ", ";
                first = false;
                jsonBody += "\"" + fieldsParam.substr(start, end - start) + "\"";
                start = end + 1;
            }
            jsonBody += "], \"maxResults\": " + std::to_string(maxResults) + "}";
            
            auto result = MakeRequest("/rest/api/3/search/jql", "POST", jsonBody);
            response = result.response;
            httpCode = result.httpCode;
            error = result.error;
        } else {
            // API v2 uses GET with query params
            std::string endpoint = "/rest/api/2/search?jql=" + UrlEncode(jql) +
                "&fields=" + fieldsParam + "&maxResults=" + std::to_string(maxResults);
            auto result = MakeRequest(endpoint, "GET");
            response = result.response;
            httpCode = result.httpCode;
            error = result.error;
        }
        
        if (!error.empty()) {
            return Result<Api::SearchResponse>::Error(error, httpCode);
        }
        
        if (httpCode >= 400) {
            return Result<Api::SearchResponse>::Error(GetHttpErrorMessage(httpCode, response), httpCode);
        }
        
        Api::SearchResponse searchResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(searchResp, response);
        if (ec) {
            return Result<Api::SearchResponse>::Error(
                "Failed to parse response: " + glz::format_error(ec, response), 0);
        }
        
        return Result<Api::SearchResponse>::Success(std::move(searchResp));
    }
    
    /**
     * Get issues assigned to the current user.
     */
    Result<std::vector<Issue>> GetMyIssues(int maxResults = 50) {
        return SearchIssues("assignee=currentUser() ORDER BY updated DESC", maxResults);
    }
    
    /**
     * Get a single issue by key.
     */
    Result<Issue> GetIssue(const std::string& issueKey) {
        if (!IsConfigured()) {
            return Result<Issue>::Error("Jira client not configured");
        }
        
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey;
        auto result = MakeRequest(endpoint, "GET");
        
        if (!result.error.empty()) {
            return Result<Issue>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<Issue>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        Api::Issue apiIssue;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(apiIssue, result.response);
        if (ec) {
            return Result<Issue>::Error(
                "Failed to parse response: " + glz::format_error(ec, result.response), 0);
        }
        
        return Result<Issue>::Success(Issue::FromApi(apiIssue, m_config.apiUrl));
    }
    
    /**
     * Create a new issue.
     * @param projectKey Project key (e.g., "PROJ")
     * @param summary Issue title
     * @param issueType Issue type name (e.g., "Task", "Bug", "Story")
     * @param description Issue description (optional)
     * @param priority Priority name (optional, e.g., "High", "Medium")
     * @return Created issue key on success
     */
    Result<std::string> CreateIssue(
        const std::string& projectKey,
        const std::string& summary,
        const std::string& issueType,
        const std::string& description = "",
        const std::string& priority = ""
    ) {
        if (!IsConfigured()) {
            return Result<std::string>::Error("Jira client not configured");
        }
        
        // Build JSON payload
        std::string json = "{\"fields\": {";
        json += "\"project\": {\"key\": \"" + EscapeJson(projectKey) + "\"},";
        json += "\"summary\": \"" + EscapeJson(summary) + "\",";
        json += "\"issuetype\": {\"name\": \"" + EscapeJson(issueType) + "\"}";
        
        if (!description.empty()) {
            json += ",\"description\": \"" + EscapeJson(description) + "\"";
        }
        if (!priority.empty()) {
            json += ",\"priority\": {\"name\": \"" + EscapeJson(priority) + "\"}";
        }
        
        json += "}}";
        
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue";
        auto result = MakeRequest(endpoint, "POST", json);
        
        if (!result.error.empty()) {
            return Result<std::string>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<std::string>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        Api::CreateIssueResponse createResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(createResp, result.response);
        if (ec || createResp.key.empty()) {
            return Result<std::string>::Error("Unexpected response from Jira API", 0);
        }
        
        return Result<std::string>::Success(createResp.key);
    }
    
    /**
     * Add a comment to an issue.
     */
    Result<std::string> AddComment(const std::string& issueKey, const std::string& body) {
        if (!IsConfigured()) {
            return Result<std::string>::Error("Jira client not configured");
        }
        
        std::string json = "{\"body\": \"" + EscapeJson(body) + "\"}";
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey + "/comment";
        auto result = MakeRequest(endpoint, "POST", json);
        
        if (!result.error.empty()) {
            return Result<std::string>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<std::string>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        Api::Comment comment;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(comment, result.response);
        if (ec) {
            return Result<std::string>::Error("Failed to parse response", 0);
        }
        
        return Result<std::string>::Success(comment.id);
    }
    
    /**
     * Get comments for an issue.
     */
    Result<std::vector<Comment>> GetComments(const std::string& issueKey, int maxResults = 50) {
        if (!IsConfigured()) {
            return Result<std::vector<Comment>>::Error("Jira client not configured");
        }
        
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey + 
            "/comment?maxResults=" + std::to_string(maxResults);
        auto result = MakeRequest(endpoint, "GET");
        
        if (!result.error.empty()) {
            return Result<std::vector<Comment>>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<std::vector<Comment>>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        Api::CommentsResponse commentsResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(commentsResp, result.response);
        if (ec) {
            return Result<std::vector<Comment>>::Error(
                "Failed to parse response: " + glz::format_error(ec, result.response), 0);
        }
        
        std::vector<Comment> comments;
        for (const auto& apiComment : commentsResp.comments) {
            comments.push_back(Comment::FromApi(apiComment));
        }
        
        return Result<std::vector<Comment>>::Success(std::move(comments));
    }
    
    /**
     * Get available transitions for an issue.
     */
    Result<std::vector<Transition>> GetTransitions(const std::string& issueKey) {
        if (!IsConfigured()) {
            return Result<std::vector<Transition>>::Error("Jira client not configured");
        }
        
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey + "/transitions";
        auto result = MakeRequest(endpoint, "GET");
        
        if (!result.error.empty()) {
            return Result<std::vector<Transition>>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<std::vector<Transition>>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        Api::TransitionsResponse transResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(transResp, result.response);
        if (ec) {
            return Result<std::vector<Transition>>::Error(
                "Failed to parse response: " + glz::format_error(ec, result.response), 0);
        }
        
        std::vector<Transition> transitions;
        for (const auto& apiTrans : transResp.transitions) {
            transitions.push_back(Transition::FromApi(apiTrans));
        }
        
        return Result<std::vector<Transition>>::Success(std::move(transitions));
    }
    
    /**
     * Transition an issue to a new status.
     */
    Result<bool> TransitionIssue(const std::string& issueKey, const std::string& transitionId) {
        if (!IsConfigured()) {
            return Result<bool>::Error("Jira client not configured");
        }
        
        std::string json = "{\"transition\": {\"id\": \"" + transitionId + "\"}}";
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey + "/transitions";
        auto result = MakeRequest(endpoint, "POST", json);
        
        if (!result.error.empty()) {
            return Result<bool>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<bool>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        return Result<bool>::Success(true);
    }
    
    /**
     * Assign an issue to a user.
     * @param issueKey Issue key
     * @param accountId User account ID (use "-1" to unassign, or empty for current user on Cloud)
     */
    Result<bool> AssignIssue(const std::string& issueKey, const std::string& accountId) {
        if (!IsConfigured()) {
            return Result<bool>::Error("Jira client not configured");
        }
        
        std::string json;
        if (accountId == "-1") {
            json = "{\"accountId\": null}";  // Unassign
        } else if (accountId.empty()) {
            json = "{}";  // Assign to self (Cloud)
        } else {
            json = "{\"accountId\": \"" + EscapeJson(accountId) + "\"}";
        }
        
        std::string endpoint = "/rest/api/" + m_config.apiVersion + "/issue/" + issueKey + "/assignee";
        auto result = MakeRequest(endpoint, "PUT", json);
        
        if (!result.error.empty()) {
            return Result<bool>::Error(result.error, result.httpCode);
        }
        
        if (result.httpCode >= 400) {
            return Result<bool>::Error(GetHttpErrorMessage(result.httpCode, result.response), result.httpCode);
        }
        
        return Result<bool>::Success(true);
    }

private:
    ClientConfig m_config;
    
    /**
     * HTTP request result.
     */
    struct RequestResult {
        std::string response;
        long httpCode = 0;
        std::string error;
    };
    
    /**
     * Make an HTTP request to the Jira API.
     */
    RequestResult MakeRequest(const std::string& endpoint, const std::string& method,
                              const std::string& body = "") {
        RequestResult result;
        
        Http::HttpClient& client = Http::getHttpClient();
        if (!client.isAvailable()) {
            result.error = "HTTP client not available";
            return result;
        }
        
        // Build request
        Http::HttpRequest req;
        req.url = m_config.apiUrl + endpoint;
        req.method = method;
        req.headers["Content-Type"] = "application/json";
        req.headers["Accept"] = "application/json";
        req.headers["Authorization"] = "Basic " + Base64Encode(m_config.user + ":" + m_config.apiToken);
        req.timeoutSeconds = m_config.timeoutSeconds;
        
        if (!body.empty()) {
            req.body = body;
        }
        
        Http::HttpResponse httpResult = client.perform(req);
        
        result.response = httpResult.body;
        result.httpCode = httpResult.statusCode;
        
        if (!httpResult.error.empty()) {
            result.error = httpResult.error;
        }
        
        return result;
    }
    
    /**
     * Get human-readable error message for HTTP status codes.
     */
    std::string GetHttpErrorMessage(long httpCode, const std::string& response) {
        switch (httpCode) {
            case 401:
                return "Authentication failed (401). Please check your credentials.";
            case 403:
                return "Access forbidden (403). Your account may not have permission.";
            case 404:
                return "Not found (404). Please check the URL or resource.";
            case 429:
                return "Rate limited (429). Please wait and try again.";
            case 500:
            case 502:
            case 503:
                return "Jira server error (" + std::to_string(httpCode) + "). Please try again later.";
            default:
                if (httpCode >= 400) {
                    // Try to extract error message from response
                    Api::ErrorResponse errResp;
                    auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(errResp, response);
                    if (!ec) {
                        if (!errResp.message.empty()) {
                            return "Error (" + std::to_string(httpCode) + "): " + errResp.message;
                        }
                        if (!errResp.errorMessages.empty()) {
                            return "Error (" + std::to_string(httpCode) + "): " + errResp.errorMessages[0];
                        }
                    }
                    return "HTTP Error " + std::to_string(httpCode);
                }
                return "";
        }
    }
    
    /**
     * Escape a string for JSON.
     */
    static std::string EscapeJson(const std::string& s) {
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
    
    /**
     * URL-encode a string.
     */
    static std::string UrlEncode(const std::string& s) {
        std::string result;
        result.reserve(s.size() * 3);
        for (unsigned char c : s) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                result += c;
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", c);
                result += buf;
            }
        }
        return result;
    }
    
    /**
     * Base64 encode a string.
     */
    static std::string Base64Encode(const std::string& input) {
        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((input.size() + 2) / 3 * 4);
        
        size_t i = 0;
        while (i < input.size()) {
            uint32_t n = static_cast<unsigned char>(input[i++]) << 16;
            if (i < input.size()) n |= static_cast<unsigned char>(input[i++]) << 8;
            if (i < input.size()) n |= static_cast<unsigned char>(input[i++]);
            
            result += chars[(n >> 18) & 63];
            result += chars[(n >> 12) & 63];
            result += (i > input.size() + 1) ? '=' : chars[(n >> 6) & 63];
            result += (i > input.size()) ? '=' : chars[n & 63];
        }
        
        return result;
    }
};

/**
 * Singleton instance of the Jira client.
 * Pre-configured from application settings.
 */
inline Client& GetClient() {
    static Client instance(ClientConfig::LoadFromConfig());
    return instance;
}

/**
 * Reload configuration for the singleton client.
 */
inline void ReloadConfig() {
    GetClient().SetConfig(ClientConfig::LoadFromConfig());
}

} // namespace Jira

#endif // JIRA_CLIENT_H
