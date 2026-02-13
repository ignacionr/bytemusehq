#ifndef GITHUB_PROJECTS_CLIENT_H
#define GITHUB_PROJECTS_CLIENT_H

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

namespace GitHub {

/**
 * GitHub Projects v2 GraphQL API response structures for Glaze JSON parsing.
 */
namespace Api {

    // -- GraphQL response envelope --
    struct GraphQLError {
        std::string message;
    };

    // -- Project item field values --
    struct FieldValueNode {
        std::optional<std::string> name;    // For SingleSelect (e.g. Status)
        std::optional<std::string> text;    // For Text fields
        std::optional<std::string> number;  // For Number fields (as string)
        std::optional<std::string> date;    // For Date fields
    };

    struct ProjectFieldValue {
        std::string fieldName;              // "Status", "Priority", etc.
        FieldValueNode value;
    };

    // -- Issue content (when an item is linked to a repo issue) --
    struct IssueContent {
        std::string title;
        std::string url;
        int number = 0;
        std::string state;                  // "OPEN", "CLOSED"
        std::optional<std::string> body;
        std::optional<std::string> updatedAt;
        std::optional<std::string> createdAt;
    };

    // -- Draft issue content --
    struct DraftIssueContent {
        std::string title;
        std::optional<std::string> body;
        std::optional<std::string> updatedAt;
        std::optional<std::string> createdAt;
    };

    // -- A single project item (parsed from GraphQL) --
    struct ProjectItem {
        std::string id;
        std::string type;                   // "ISSUE", "PULL_REQUEST", "DRAFT_ISSUE", "REDACTED"
        std::optional<IssueContent> issueContent;
        std::optional<DraftIssueContent> draftContent;

        // Field values extracted after parsing
        std::string status;
        std::string priority;
        std::string assignee;
    };

    // -- Status option in a project --
    struct StatusOption {
        std::string id;
        std::string name;
    };

    // -- Project metadata --
    struct ProjectInfo {
        std::string id;
        std::string title;
        std::string url;
        int number = 0;
    };

} // namespace Api

/**
 * Simplified issue structure for general use (mirrors Jira::Issue).
 */
struct Issue {
    std::string key;           // e.g., "owner/repo#123" or "DRAFT-<id>"
    std::string summary;       // Issue title
    std::string description;   // Issue body
    std::string status;        // Project status field value
    std::string priority;      // Project priority field value
    std::string type;          // "Issue", "Pull Request", "Draft"
    std::string assignee;      // Assignee login
    std::string reporter;      // Author login (if available)
    std::string updated;       // Last updated timestamp
    std::string url;           // Web URL to the issue
    std::string projectItemId; // Internal project item node ID (for mutations)
};

/**
 * Comment structure.
 */
struct Comment {
    std::string id;
    std::string body;
    std::string author;
    std::string created;
    std::string updated;
};

/**
 * Status option (analogous to Jira Transition).
 */
struct StatusOption {
    std::string id;
    std::string name;
};

/**
 * Configuration for the GitHub Projects client.
 */
struct ClientConfig {
    std::string token;           // Personal access token (classic or fine-grained)
    std::string owner;           // GitHub user or organization
    int projectNumber = 0;       // Project number (visible in URL)
    std::string ownerType = "organization"; // "organization" or "user"
    int timeoutSeconds = 30;

    bool isValid() const {
        return !token.empty() && !owner.empty() && projectNumber > 0;
    }

    /**
     * Load configuration from Config singleton.
     */
    static ClientConfig LoadFromConfig() {
        auto& config = Config::Instance();
        ClientConfig cfg;
        cfg.token = config.GetString("github.token", "").ToStdString();
        cfg.owner = config.GetString("github.owner", "").ToStdString();
        cfg.projectNumber = config.GetInt("github.projectNumber", 0);
        cfg.ownerType = config.GetString("github.ownerType", "organization").ToStdString();
        return cfg;
    }
};

/**
 * Result of a GitHub API operation.
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
 * GitHub Projects v2 API client.
 * Uses the GitHub GraphQL API to interact with Projects (v2).
 */
class ProjectsClient {
public:
    ProjectsClient() = default;
    explicit ProjectsClient(const ClientConfig& config) : m_config(config) {}

    void SetConfig(const ClientConfig& config) { m_config = config; }
    const ClientConfig& GetConfig() const { return m_config; }
    bool IsConfigured() const { return m_config.isValid(); }

    // ========================================================================
    // Queries
    // ========================================================================

    /**
     * Get project metadata (ID, title, URL).
     */
    Result<Api::ProjectInfo> GetProjectInfo() {
        if (!IsConfigured()) return Result<Api::ProjectInfo>::Error("GitHub Projects client not configured");

        std::string ownerField = (m_config.ownerType == "user") ? "user" : "organization";
        std::string query = R"(query{)" + ownerField + R"((login:")" + EscapeGql(m_config.owner) +
            R"("){projectV2(number:)" + std::to_string(m_config.projectNumber) +
            R"(){id title url number}}})";

        auto result = GraphQL(query);
        if (!result.success) return Result<Api::ProjectInfo>::Error(result.error, result.httpCode);

        // Parse: data.<ownerField>.projectV2
        Api::ProjectInfo info;
        auto& body = result.data;

        // Simple JSON path extraction
        info.id = ExtractJsonString(body, "id", 3);   // 3rd level
        info.title = ExtractJsonString(body, "title");
        info.url = ExtractJsonString(body, "url", 3);
        info.number = m_config.projectNumber;

        // Better: use glaze for structured parse
        struct PV2 { std::string id; std::string title; std::string url; int number = 0; };
        struct Owner { PV2 projectV2; };
        struct Data { Owner organization; Owner user; };
        struct Resp { Data data; };

        Resp resp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(resp, body);
        if (ec) {
            return Result<Api::ProjectInfo>::Error("Failed to parse project info: " + glz::format_error(ec, body));
        }

        auto& pv2 = (m_config.ownerType == "user") ? resp.data.user.projectV2 : resp.data.organization.projectV2;
        info.id = pv2.id;
        info.title = pv2.title;
        info.url = pv2.url;
        info.number = pv2.number;

        if (info.id.empty()) {
            return Result<Api::ProjectInfo>::Error("Project not found");
        }
        return Result<Api::ProjectInfo>::Success(std::move(info));
    }

    /**
     * List items in the project (issues, PRs, draft issues).
     * @param maxResults Maximum number of items to return
     * @param statusFilter Optional status name to filter by
     */
    Result<std::vector<Issue>> ListItems(int maxResults = 50, const std::string& statusFilter = "") {
        if (!IsConfigured()) return Result<std::vector<Issue>>::Error("GitHub Projects client not configured");

        std::string ownerField = (m_config.ownerType == "user") ? "user" : "organization";

        // GraphQL query to fetch project items with field values
        std::string query = R"(query{)" + ownerField + R"((login:")" + EscapeGql(m_config.owner) +
            R"("){projectV2(number:)" + std::to_string(m_config.projectNumber) +
            R"(){items(first:)" + std::to_string(maxResults) + R"(,orderBy:{field:POSITION,direction:ASC}){nodes{
id type
fieldValues(first:20){nodes{
... on ProjectV2ItemFieldSingleSelectValue{field{... on ProjectV2SingleSelectField{name}} name}
... on ProjectV2ItemFieldTextValue{field{... on ProjectV2Field{name}} text}
... on ProjectV2ItemFieldNumberValue{field{... on ProjectV2Field{name}} number}
... on ProjectV2ItemFieldDateValue{field{... on ProjectV2Field{name}} date}
... on ProjectV2ItemFieldUserValue{field{... on ProjectV2Field{name}} users(first:1){nodes{login}}}
}}
content{
... on Issue{title url number state body updatedAt createdAt assignees(first:1){nodes{login}} author{login}}
... on PullRequest{title url number state body updatedAt createdAt assignees(first:1){nodes{login}} author{login}}
... on DraftIssue{title body updatedAt createdAt}
}
}}}})";

        auto result = GraphQL(query);
        if (!result.success) return Result<std::vector<Issue>>::Error(result.error, result.httpCode);

        return ParseItemsResponse(result.data, statusFilter);
    }

    /**
     * Get items assigned to a specific user.
     */
    Result<std::vector<Issue>> GetMyItems(int maxResults = 50) {
        // Fetch all items then filter by assignee. The GraphQL API for Projects v2
        // doesn't have a direct assignee filter on items, so we fetch and filter.
        auto result = ListItems(std::min(maxResults * 3, 100)); // fetch more to filter
        if (!result.success) return result;

        // We need to know the authenticated user's login
        auto userResult = GetAuthenticatedUser();
        if (!userResult.success) return Result<std::vector<Issue>>::Error(userResult.error);

        std::vector<Issue> filtered;
        for (auto& item : result.data) {
            if (item.assignee == userResult.data || item.assignee.empty()) {
                // Include assigned-to-me and unassigned (drafts without assignee)
            }
            if (item.assignee == userResult.data) {
                filtered.push_back(std::move(item));
                if (static_cast<int>(filtered.size()) >= maxResults) break;
            }
        }
        return Result<std::vector<Issue>>::Success(std::move(filtered));
    }

    /**
     * Get a single issue by its repo issue number.
     * @param issueRef Format: "owner/repo#123" or just "#123" (uses first repo found)
     */
    Result<Issue> GetIssue(const std::string& issueRef) {
        if (!IsConfigured()) return Result<Issue>::Error("GitHub Projects client not configured");

        // Parse the reference
        std::string owner, repo;
        int number = 0;

        auto hashPos = issueRef.find('#');
        if (hashPos != std::string::npos) {
            std::string prefix = issueRef.substr(0, hashPos);
            auto slashPos = prefix.find('/');
            if (slashPos != std::string::npos) {
                owner = prefix.substr(0, slashPos);
                repo = prefix.substr(slashPos + 1);
            }
            try { number = std::stoi(issueRef.substr(hashPos + 1)); } catch (...) {}
        }

        if (number == 0) {
            return Result<Issue>::Error("Invalid issue reference. Use format: owner/repo#123");
        }

        // If no owner/repo provided, try to get it from the REST API
        if (owner.empty() || repo.empty()) {
            return Result<Issue>::Error("Please use full reference format: owner/repo#123");
        }

        // Fetch the issue via REST API
        std::string endpoint = "/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number);
        auto result = RestGet(endpoint);
        if (!result.success) return Result<Issue>::Error(result.error, result.httpCode);

        // Parse response
        struct IssueResp {
            int number = 0;
            std::string title;
            std::optional<std::string> body;
            std::string state;
            std::string html_url;
            std::string updated_at;
            std::string created_at;
            struct User { std::string login; };
            std::optional<User> user;
            std::optional<std::vector<User>> assignees;
            struct Label { std::string name; };
            std::optional<std::vector<Label>> labels;
        };

        IssueResp issueResp;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(issueResp, result.data);
        if (ec) return Result<Issue>::Error("Failed to parse issue response");

        Issue issue;
        issue.key = owner + "/" + repo + "#" + std::to_string(issueResp.number);
        issue.summary = issueResp.title;
        issue.description = issueResp.body.value_or("");
        issue.status = issueResp.state == "open" ? "Open" : "Closed";
        issue.type = "Issue";
        issue.url = issueResp.html_url;
        issue.updated = issueResp.updated_at;
        if (issueResp.user) issue.reporter = issueResp.user->login;
        if (issueResp.assignees && !issueResp.assignees->empty()) {
            issue.assignee = issueResp.assignees->front().login;
        }
        // Extract priority from labels
        if (issueResp.labels) {
            for (auto& label : *issueResp.labels) {
                auto lower = label.name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower.find("priority") != std::string::npos ||
                    lower.find("p0") != std::string::npos || lower.find("p1") != std::string::npos ||
                    lower.find("critical") != std::string::npos || lower.find("urgent") != std::string::npos) {
                    issue.priority = label.name;
                    break;
                }
            }
        }

        return Result<Issue>::Success(std::move(issue));
    }

    /**
     * Get available status options for the project.
     */
    Result<std::vector<StatusOption>> GetStatusOptions() {
        if (!IsConfigured()) return Result<std::vector<StatusOption>>::Error("Not configured");

        std::string ownerField = (m_config.ownerType == "user") ? "user" : "organization";
        std::string query = R"(query{)" + ownerField + R"((login:")" + EscapeGql(m_config.owner) +
            R"("){projectV2(number:)" + std::to_string(m_config.projectNumber) +
            R"(){fields(first:30){nodes{... on ProjectV2SingleSelectField{id name options{id name}}}}}})";

        auto result = GraphQL(query);
        if (!result.success) return Result<std::vector<StatusOption>>::Error(result.error);

        // Parse the response to find the "Status" field and its options
        std::vector<StatusOption> options;

        // Look for "Status" field options in the JSON
        // Simple extraction since Glaze doesn't handle GraphQL union types natively
        auto& body = result.data;
        // Find the Status field's options
        size_t namePos = 0;
        while ((namePos = body.find("\"name\"", namePos)) != std::string::npos) {
            auto valStart = body.find(':', namePos) + 1;
            while (valStart < body.size() && body[valStart] == ' ') valStart++;
            if (body[valStart] == '"') {
                auto valEnd = body.find('"', valStart + 1);
                std::string fieldName = body.substr(valStart + 1, valEnd - valStart - 1);
                if (fieldName == "Status") {
                    // Found the Status field — extract options array
                    auto optionsPos = body.find("\"options\"", valEnd);
                    if (optionsPos != std::string::npos) {
                        auto arrStart = body.find('[', optionsPos);
                        auto arrEnd = body.find(']', arrStart);
                        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
                            std::string optArr = body.substr(arrStart, arrEnd - arrStart + 1);

                            struct Opt { std::string id; std::string name; };
                            std::vector<Opt> opts;
                            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(opts, optArr);
                            if (!ec) {
                                for (auto& o : opts) {
                                    options.push_back({o.id, o.name});
                                }
                            }
                        }
                    }
                    break;
                }
            }
            namePos++;
        }

        return Result<std::vector<StatusOption>>::Success(std::move(options));
    }

    // ========================================================================
    // Mutations
    // ========================================================================

    /**
     * Create a draft issue in the project.
     */
    Result<std::string> CreateDraftIssue(const std::string& title, const std::string& body = "") {
        if (!IsConfigured()) return Result<std::string>::Error("Not configured");

        // First get the project node ID
        auto projResult = GetProjectInfo();
        if (!projResult.success) return Result<std::string>::Error(projResult.error);

        std::string bodyGql = body.empty() ? "" : R"(,body:")" + EscapeGql(body) + R"(")";
        std::string mutation = R"(mutation{addProjectV2DraftIssue(input:{
projectId:")" + projResult.data.id + R"(",title:")" + EscapeGql(title) + R"(")" + bodyGql +
            R"(}){projectItem{id}}})";

        auto result = GraphQL(mutation);
        if (!result.success) return Result<std::string>::Error(result.error, result.httpCode);

        // Extract the new item ID
        std::string itemId = ExtractJsonString(result.data, "id", 4);
        if (itemId.empty()) {
            return Result<std::string>::Error("Failed to extract created item ID");
        }

        return Result<std::string>::Success(std::move(itemId));
    }

    /**
     * Add a comment to a repo issue.
     * @param issueRef Format: "owner/repo#123"
     */
    Result<std::string> AddComment(const std::string& issueRef, const std::string& body) {
        if (!IsConfigured()) return Result<std::string>::Error("Not configured");

        // Parse reference
        std::string owner, repo;
        int number = 0;
        auto hashPos = issueRef.find('#');
        if (hashPos != std::string::npos) {
            std::string prefix = issueRef.substr(0, hashPos);
            auto slashPos = prefix.find('/');
            if (slashPos != std::string::npos) {
                owner = prefix.substr(0, slashPos);
                repo = prefix.substr(slashPos + 1);
            }
            try { number = std::stoi(issueRef.substr(hashPos + 1)); } catch (...) {}
        }

        if (owner.empty() || repo.empty() || number == 0) {
            return Result<std::string>::Error("Invalid issue reference. Use: owner/repo#123");
        }

        std::string endpoint = "/repos/" + owner + "/" + repo + "/issues/" + std::to_string(number) + "/comments";
        std::string json = R"({"body":")" + EscapeJson(body) + R"("})";

        auto result = RestPost(endpoint, json);
        if (!result.success) return Result<std::string>::Error(result.error, result.httpCode);

        // Extract comment ID
        struct CommentResp { int id = 0; };
        CommentResp cr;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(cr, result.data);
        if (ec) return Result<std::string>::Error("Failed to parse comment response");

        return Result<std::string>::Success(std::to_string(cr.id));
    }

    /**
     * Get comments on a repo issue.
     */
    Result<std::vector<Comment>> GetComments(const std::string& issueRef, int maxResults = 30) {
        if (!IsConfigured()) return Result<std::vector<Comment>>::Error("Not configured");

        std::string owner, repo;
        int number = 0;
        auto hashPos = issueRef.find('#');
        if (hashPos != std::string::npos) {
            std::string prefix = issueRef.substr(0, hashPos);
            auto slashPos = prefix.find('/');
            if (slashPos != std::string::npos) {
                owner = prefix.substr(0, slashPos);
                repo = prefix.substr(slashPos + 1);
            }
            try { number = std::stoi(issueRef.substr(hashPos + 1)); } catch (...) {}
        }

        if (owner.empty() || repo.empty() || number == 0) {
            return Result<std::vector<Comment>>::Error("Invalid issue reference. Use: owner/repo#123");
        }

        std::string endpoint = "/repos/" + owner + "/" + repo + "/issues/" +
            std::to_string(number) + "/comments?per_page=" + std::to_string(maxResults);

        auto result = RestGet(endpoint);
        if (!result.success) return Result<std::vector<Comment>>::Error(result.error);

        struct CommentApi {
            int id = 0;
            std::string body;
            struct User { std::string login; };
            User user;
            std::string created_at;
            std::string updated_at;
        };

        std::vector<CommentApi> apiComments;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(apiComments, result.data);
        if (ec) return Result<std::vector<Comment>>::Error("Failed to parse comments response");

        std::vector<Comment> comments;
        for (auto& c : apiComments) {
            Comment comment;
            comment.id = std::to_string(c.id);
            comment.body = c.body;
            comment.author = c.user.login;
            comment.created = c.created_at;
            comment.updated = c.updated_at;
            comments.push_back(std::move(comment));
        }

        return Result<std::vector<Comment>>::Success(std::move(comments));
    }

    /**
     * Update the status of a project item.
     * @param itemId The project item node ID
     * @param statusOptionId The status option ID to set
     */
    Result<bool> UpdateItemStatus(const std::string& itemId, const std::string& statusOptionId) {
        if (!IsConfigured()) return Result<bool>::Error("Not configured");

        // Get project ID and status field ID
        auto projResult = GetProjectInfo();
        if (!projResult.success) return Result<bool>::Error(projResult.error);

        auto statusFieldId = GetStatusFieldId();
        if (!statusFieldId.success) return Result<bool>::Error(statusFieldId.error);

        std::string mutation = R"(mutation{updateProjectV2ItemFieldValue(input:{
projectId:")" + projResult.data.id + R"(",itemId:")" + itemId +
            R"(",fieldId:")" + statusFieldId.data +
            R"(",value:{singleSelectOptionId:")" + statusOptionId + R"("}}){projectV2Item{id}}})";

        auto result = GraphQL(mutation);
        if (!result.success) return Result<bool>::Error(result.error, result.httpCode);

        return Result<bool>::Success(true);
    }

    /**
     * Get the authenticated user's login.
     */
    Result<std::string> GetAuthenticatedUser() {
        auto result = RestGet("/user");
        if (!result.success) return Result<std::string>::Error(result.error);

        struct UserResp { std::string login; };
        UserResp user;
        auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(user, result.data);
        if (ec || user.login.empty()) return Result<std::string>::Error("Failed to get authenticated user");

        return Result<std::string>::Success(user.login);
    }

private:
    ClientConfig m_config;

    struct RequestResult {
        bool success = false;
        std::string data;
        std::string error;
        long httpCode = 0;
    };

    /**
     * Get the Status field node ID.
     */
    Result<std::string> GetStatusFieldId() {
        std::string ownerField = (m_config.ownerType == "user") ? "user" : "organization";
        std::string query = R"(query{)" + ownerField + R"((login:")" + EscapeGql(m_config.owner) +
            R"("){projectV2(number:)" + std::to_string(m_config.projectNumber) +
            R"(){fields(first:30){nodes{... on ProjectV2SingleSelectField{id name}}}}}})";

        auto result = GraphQL(query);
        if (!result.success) return Result<std::string>::Error(result.error);

        // Find the "Status" field ID
        size_t pos = 0;
        while ((pos = result.data.find("\"name\"", pos)) != std::string::npos) {
            auto valStart = result.data.find('"', result.data.find(':', pos) + 1);
            if (valStart == std::string::npos) break;
            auto valEnd = result.data.find('"', valStart + 1);
            std::string name = result.data.substr(valStart + 1, valEnd - valStart - 1);

            if (name == "Status") {
                // Look for "id" before this "name"
                // The id should be nearby. Search backwards from pos.
                auto idSearchStart = (pos > 200) ? pos - 200 : 0;
                auto idPos = result.data.rfind("\"id\"", pos);
                if (idPos != std::string::npos && idPos >= idSearchStart) {
                    auto idValStart = result.data.find('"', result.data.find(':', idPos) + 1);
                    auto idValEnd = result.data.find('"', idValStart + 1);
                    return Result<std::string>::Success(
                        result.data.substr(idValStart + 1, idValEnd - idValStart - 1));
                }
            }
            pos = valEnd + 1;
        }

        return Result<std::string>::Error("Status field not found in project");
    }

    /**
     * Execute a GraphQL query/mutation against the GitHub API.
     */
    RequestResult GraphQL(const std::string& queryOrMutation) {
        RequestResult result;

        Http::HttpClient& client = Http::getHttpClient();
        if (!client.isAvailable()) {
            result.error = "HTTP client not available";
            return result;
        }

        Http::HttpRequest req;
        req.url = "https://api.github.com/graphql";
        req.method = "POST";
        req.headers["Content-Type"] = "application/json";
        req.headers["Accept"] = "application/json";
        req.headers["Authorization"] = "Bearer " + m_config.token;
        req.headers["User-Agent"] = "ByteMuseHQ";
        req.timeoutSeconds = m_config.timeoutSeconds;

        // Wrap in {"query": "..."} envelope
        req.body = R"({"query":")" + EscapeJson(queryOrMutation) + R"("})";

        Http::HttpResponse httpResult = client.perform(req);
        result.httpCode = httpResult.statusCode;
        result.data = httpResult.body;

        if (!httpResult.error.empty()) {
            result.error = httpResult.error;
            return result;
        }

        if (httpResult.statusCode >= 400) {
            result.error = GetHttpErrorMessage(httpResult.statusCode, httpResult.body);
            return result;
        }

        // Check for GraphQL-level errors
        if (httpResult.body.find("\"errors\"") != std::string::npos) {
            // Try to extract error message
            struct ErrResp { std::vector<Api::GraphQLError> errors; };
            ErrResp errResp;
            auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(errResp, httpResult.body);
            if (!ec && !errResp.errors.empty()) {
                result.error = errResp.errors[0].message;
                // Still mark success=false if only errors (no data)
                if (httpResult.body.find("\"data\":null") != std::string::npos) {
                    return result;
                }
            }
        }

        result.success = true;
        return result;
    }

    /**
     * REST API GET request.
     */
    RequestResult RestGet(const std::string& endpoint) {
        RequestResult result;

        Http::HttpClient& client = Http::getHttpClient();
        if (!client.isAvailable()) {
            result.error = "HTTP client not available";
            return result;
        }

        Http::HttpRequest req;
        req.url = "https://api.github.com" + endpoint;
        req.method = "GET";
        req.headers["Accept"] = "application/vnd.github+json";
        req.headers["Authorization"] = "Bearer " + m_config.token;
        req.headers["X-GitHub-Api-Version"] = "2022-11-28";
        req.headers["User-Agent"] = "ByteMuseHQ";
        req.timeoutSeconds = m_config.timeoutSeconds;

        Http::HttpResponse httpResult = client.perform(req);
        result.httpCode = httpResult.statusCode;
        result.data = httpResult.body;

        if (!httpResult.error.empty()) {
            result.error = httpResult.error;
            return result;
        }

        if (httpResult.statusCode >= 400) {
            result.error = GetHttpErrorMessage(httpResult.statusCode, httpResult.body);
            return result;
        }

        result.success = true;
        return result;
    }

    /**
     * REST API POST request.
     */
    RequestResult RestPost(const std::string& endpoint, const std::string& body) {
        RequestResult result;

        Http::HttpClient& client = Http::getHttpClient();
        if (!client.isAvailable()) {
            result.error = "HTTP client not available";
            return result;
        }

        Http::HttpRequest req;
        req.url = "https://api.github.com" + endpoint;
        req.method = "POST";
        req.headers["Content-Type"] = "application/json";
        req.headers["Accept"] = "application/vnd.github+json";
        req.headers["Authorization"] = "Bearer " + m_config.token;
        req.headers["X-GitHub-Api-Version"] = "2022-11-28";
        req.headers["User-Agent"] = "ByteMuseHQ";
        req.timeoutSeconds = m_config.timeoutSeconds;
        req.body = body;

        Http::HttpResponse httpResult = client.perform(req);
        result.httpCode = httpResult.statusCode;
        result.data = httpResult.body;

        if (!httpResult.error.empty()) {
            result.error = httpResult.error;
            return result;
        }

        if (httpResult.statusCode >= 400) {
            result.error = GetHttpErrorMessage(httpResult.statusCode, httpResult.body);
            return result;
        }

        result.success = true;
        return result;
    }

    /**
     * Parse the items response from ListItems GraphQL query.
     */
    Result<std::vector<Issue>> ParseItemsResponse(const std::string& body, const std::string& statusFilter) {
        // The GraphQL response is deeply nested. We'll do manual JSON navigation
        // since the union types in GraphQL don't map cleanly to static structs.
        std::vector<Issue> issues;

        // Find the items.nodes array
        auto nodesPos = body.find("\"nodes\"");
        if (nodesPos == std::string::npos) {
            return Result<std::vector<Issue>>::Error("Unexpected response format");
        }
        // Skip to the items-level nodes (second occurrence is the items.nodes)
        auto itemsPos = body.find("\"items\"");
        if (itemsPos == std::string::npos) {
            return Result<std::vector<Issue>>::Error("No items found in response");
        }

        // We'll parse individual item objects using bracket matching
        auto arrStart = body.find('[', body.find("\"nodes\"", itemsPos));
        if (arrStart == std::string::npos) {
            return Result<std::vector<Issue>>::Success({}); // Empty project
        }

        // Walk through each item in the array
        int depth = 0;
        size_t itemStart = 0;
        for (size_t i = arrStart; i < body.size(); i++) {
            if (body[i] == '{') {
                if (depth == 1) itemStart = i; // Top-level item object
                depth++;
            } else if (body[i] == '}') {
                depth--;
                if (depth == 1) {
                    // We have a complete item object
                    std::string itemJson = body.substr(itemStart, i - itemStart + 1);
                    auto issue = ParseSingleItem(itemJson);
                    if (!issue.summary.empty()) {
                        if (statusFilter.empty() || issue.status == statusFilter) {
                            issues.push_back(std::move(issue));
                        }
                    }
                }
                if (depth == 0) break; // End of array
            }
        }

        return Result<std::vector<Issue>>::Success(std::move(issues));
    }

    /**
     * Parse a single project item from its JSON fragment.
     */
    Issue ParseSingleItem(const std::string& json) {
        Issue issue;

        issue.projectItemId = ExtractJsonString(json, "id");
        std::string type = ExtractJsonString(json, "type");

        if (type == "ISSUE" || type == "PULL_REQUEST") {
            issue.type = (type == "ISSUE") ? "Issue" : "Pull Request";
            // Extract content fields
            issue.summary = ExtractJsonString(json, "title");
            issue.url = ExtractJsonString(json, "url");
            issue.description = ExtractJsonString(json, "body");
            issue.updated = ExtractJsonString(json, "updatedAt");

            // Build key from URL: extract owner/repo#number
            if (!issue.url.empty()) {
                // URL format: https://github.com/owner/repo/issues/123
                std::string path = issue.url;
                auto ghPos = path.find("github.com/");
                if (ghPos != std::string::npos) {
                    path = path.substr(ghPos + 11); // after "github.com/"
                    auto parts = SplitString(path, '/');
                    if (parts.size() >= 4) {
                        issue.key = parts[0] + "/" + parts[1] + "#" + parts[3];
                    }
                }
            }

            // Extract state
            std::string state = ExtractJsonString(json, "state");
            if (!state.empty()) {
                // Will be overridden by project Status field if present
            }

            // Extract assignee from nested assignees.nodes[0].login
            auto assigneesPos = json.find("\"assignees\"");
            if (assigneesPos != std::string::npos) {
                auto loginPos = json.find("\"login\"", assigneesPos);
                if (loginPos != std::string::npos) {
                    issue.assignee = ExtractJsonStringAt(json, loginPos);
                }
            }

            // Extract author
            auto authorPos = json.find("\"author\"");
            if (authorPos != std::string::npos) {
                auto loginPos = json.find("\"login\"", authorPos);
                if (loginPos != std::string::npos) {
                    issue.reporter = ExtractJsonStringAt(json, loginPos);
                }
            }

        } else if (type == "DRAFT_ISSUE") {
            issue.type = "Draft";
            issue.summary = ExtractJsonString(json, "title");
            issue.description = ExtractJsonString(json, "body");
            issue.updated = ExtractJsonString(json, "updatedAt");
            issue.key = "DRAFT-" + issue.projectItemId.substr(0, 8);
        } else {
            return issue; // REDACTED or unknown
        }

        // Extract field values (Status, Priority, etc.)
        auto fieldValuesPos = json.find("\"fieldValues\"");
        if (fieldValuesPos != std::string::npos) {
            // Find all field name/value pairs
            size_t searchFrom = fieldValuesPos;
            while (true) {
                // Look for field name patterns
                auto namePos = json.find("\"name\"", searchFrom);
                if (namePos == std::string::npos || namePos > json.size() - 10) break;

                std::string fieldOrValue = ExtractJsonStringAt(json, namePos);

                // Check if this is a field definition (has another "name" nearby for the value)
                // The pattern is: field{name:"FieldName"} name:"ValueName"  (for SingleSelect)
                // or: field{name:"FieldName"} text:"ValueText" (for Text)

                // Look for common field patterns
                if (fieldOrValue == "Status") {
                    // Find the next "name" which is the status value
                    auto nextName = json.find("\"name\"", namePos + 8);
                    if (nextName != std::string::npos && nextName < namePos + 100) {
                        issue.status = ExtractJsonStringAt(json, nextName);
                    }
                } else if (fieldOrValue == "Priority") {
                    auto nextName = json.find("\"name\"", namePos + 10);
                    if (nextName != std::string::npos && nextName < namePos + 100) {
                        issue.priority = ExtractJsonStringAt(json, nextName);
                    }
                }

                searchFrom = namePos + 6;
            }
        }

        return issue;
    }

    /**
     * Extract a JSON string value by its key name.
     * Simple extraction for non-nested results.
     */
    static std::string ExtractJsonString(const std::string& json, const std::string& key, int skip = 0) {
        std::string search = "\"" + key + "\"";
        size_t pos = 0;
        for (int i = 0; i <= skip; i++) {
            pos = json.find(search, pos);
            if (pos == std::string::npos) return "";
            if (i < skip) pos += search.size();
        }
        return ExtractJsonStringAt(json, pos);
    }

    /**
     * Extract a JSON string value starting from a position where "key" was found.
     */
    static std::string ExtractJsonStringAt(const std::string& json, size_t keyPos) {
        auto colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return "";

        // Skip whitespace
        auto valStart = colonPos + 1;
        while (valStart < json.size() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;

        if (valStart >= json.size() || json[valStart] != '"') return "";

        // Find end of string, handling escapes
        std::string result;
        for (size_t i = valStart + 1; i < json.size(); i++) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                i++;
                switch (json[i]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += json[i]; break;
                }
            } else if (json[i] == '"') {
                break;
            } else {
                result += json[i];
            }
        }
        return result;
    }

    static std::vector<std::string> SplitString(const std::string& s, char delimiter) {
        std::vector<std::string> parts;
        std::string part;
        for (char c : s) {
            if (c == delimiter) {
                if (!part.empty()) parts.push_back(part);
                part.clear();
            } else {
                part += c;
            }
        }
        if (!part.empty()) parts.push_back(part);
        return parts;
    }

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

    static std::string EscapeGql(const std::string& s) {
        // For GraphQL string literals inside query strings
        return EscapeJson(s);
    }

    std::string GetHttpErrorMessage(long httpCode, const std::string& response) {
        switch (httpCode) {
            case 401: return "Authentication failed (401). Check your GitHub token.";
            case 403: return "Access forbidden (403). Token may lack required scopes (project, repo).";
            case 404: return "Not found (404). Check the owner, repo, or project number.";
            case 422: return "Validation failed (422). " + ExtractJsonString(response, "message");
            case 429: return "Rate limited (429). Please wait and try again.";
            default:
                if (httpCode >= 500)
                    return "GitHub server error (" + std::to_string(httpCode) + "). Try again later.";
                if (httpCode >= 400) {
                    auto msg = ExtractJsonString(response, "message");
                    if (!msg.empty()) return "Error (" + std::to_string(httpCode) + "): " + msg;
                    return "HTTP Error " + std::to_string(httpCode);
                }
                return "";
        }
    }
};

/**
 * Singleton instance of the GitHub Projects client.
 */
inline ProjectsClient& GetClient() {
    static ProjectsClient instance(ClientConfig::LoadFromConfig());
    return instance;
}

inline void ReloadConfig() {
    GetClient().SetConfig(ClientConfig::LoadFromConfig());
}

} // namespace GitHub

#endif // GITHUB_PROJECTS_CLIENT_H
