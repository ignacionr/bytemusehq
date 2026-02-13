#ifndef MCP_GITHUB_PROJECTS_H
#define MCP_GITHUB_PROJECTS_H

#include "mcp.h"
#include "../github/github_projects_client.h"
#include <wx/log.h>

namespace MCP {

/**
 * GitHub Projects MCP Provider.
 *
 * Provides tools for the AI to interact with GitHub Projects (v2).
 * This enables the AI to search, view, create, and manage project items.
 *
 * Available tools:
 * - github_list_items: List items in the project
 * - github_get_my_items: Get items assigned to the authenticated user
 * - github_get_issue: Get details of a specific issue
 * - github_create_draft: Create a new draft issue in the project
 * - github_add_comment: Add a comment to an issue
 * - github_get_comments: Get comments on an issue
 * - github_get_status_options: Get available status options
 * - github_update_status: Update the status of a project item
 */
class GitHubProjectsProvider : public Provider {
public:
    GitHubProjectsProvider() : m_client(GitHub::ClientConfig::LoadFromConfig()) {
        auto cfg = m_client.GetConfig();
        wxLogDebug("MCP GitHub Projects: Initialized with owner='%s', project=%d, configured=%s",
                   cfg.owner.c_str(), cfg.projectNumber,
                   m_client.IsConfigured() ? "yes" : "no");
    }

    explicit GitHubProjectsProvider(const GitHub::ClientConfig& config) : m_client(config) {}

    std::string getId() const override {
        return "mcp.github_projects";
    }

    std::string getName() const override {
        return "GitHub Projects";
    }

    std::string getDescription() const override {
        return "Provides access to GitHub Projects (v2) issue tracking";
    }

    void setConfig(const GitHub::ClientConfig& config) {
        m_client.SetConfig(config);
    }

    void reloadConfig() {
        m_client.SetConfig(GitHub::ClientConfig::LoadFromConfig());
    }

    bool isConfigured() const {
        return m_client.IsConfigured();
    }

    bool isEnabled() const override {
        return m_enabled && m_client.IsConfigured();
    }

    std::vector<ToolDefinition> getTools() const override {
        std::vector<ToolDefinition> tools;

        // github_list_items
        {
            ToolDefinition tool;
            tool.name = "github_list_items";
            tool.description = "List items in the GitHub Project (issues, PRs, drafts). "
                             "Optionally filter by status (e.g., 'Todo', 'In Progress', 'Done').";
            tool.parameters = {
                {"status_filter", "string", "Filter by status name (optional)", false},
                {"max_results", "number", "Maximum number of results to return (default: 30)", false}
            };
            tools.push_back(tool);
        }

        // github_get_my_items
        {
            ToolDefinition tool;
            tool.name = "github_get_my_items";
            tool.description = "Get GitHub Project items assigned to the authenticated user.";
            tool.parameters = {
                {"max_results", "number", "Maximum number of results to return (default: 20)", false}
            };
            tools.push_back(tool);
        }

        // github_get_issue
        {
            ToolDefinition tool;
            tool.name = "github_get_issue";
            tool.description = "Get details of a specific GitHub issue.";
            tool.parameters = {
                {"issue_ref", "string", "Issue reference (e.g., 'owner/repo#123')", true}
            };
            tools.push_back(tool);
        }

        // github_create_draft
        {
            ToolDefinition tool;
            tool.name = "github_create_draft";
            tool.description = "Create a new draft issue in the GitHub Project.";
            tool.parameters = {
                {"title", "string", "The title of the draft issue", true},
                {"body", "string", "The body/description of the draft issue (optional)", false}
            };
            tools.push_back(tool);
        }

        // github_add_comment
        {
            ToolDefinition tool;
            tool.name = "github_add_comment";
            tool.description = "Add a comment to a GitHub issue.";
            tool.parameters = {
                {"issue_ref", "string", "Issue reference (e.g., 'owner/repo#123')", true},
                {"comment", "string", "The comment text to add", true}
            };
            tools.push_back(tool);
        }

        // github_get_comments
        {
            ToolDefinition tool;
            tool.name = "github_get_comments";
            tool.description = "Get comments on a GitHub issue.";
            tool.parameters = {
                {"issue_ref", "string", "Issue reference (e.g., 'owner/repo#123')", true},
                {"max_results", "number", "Maximum number of comments (default: 20)", false}
            };
            tools.push_back(tool);
        }

        // github_get_status_options
        {
            ToolDefinition tool;
            tool.name = "github_get_status_options";
            tool.description = "Get available status options for the GitHub Project. "
                             "Use this to see what statuses an item can be moved to.";
            tool.parameters = {};
            tools.push_back(tool);
        }

        // github_update_status
        {
            ToolDefinition tool;
            tool.name = "github_update_status";
            tool.description = "Update the status of a GitHub Project item. "
                             "Use github_get_status_options first to get valid status option IDs.";
            tool.parameters = {
                {"item_id", "string", "The project item node ID", true},
                {"status_option_id", "string", "The status option ID to set", true}
            };
            tools.push_back(tool);
        }

        return tools;
    }

    ToolResult executeTool(const std::string& toolName, const Value& arguments) override {
        if (!m_client.IsConfigured()) {
            return ToolResult::Error("GitHub Projects is not configured. "
                "Please set github.token, github.owner, and github.projectNumber in settings.");
        }

        if (toolName == "github_list_items") return listItems(arguments);
        if (toolName == "github_get_my_items") return getMyItems(arguments);
        if (toolName == "github_get_issue") return getIssue(arguments);
        if (toolName == "github_create_draft") return createDraft(arguments);
        if (toolName == "github_add_comment") return addComment(arguments);
        if (toolName == "github_get_comments") return getComments(arguments);
        if (toolName == "github_get_status_options") return getStatusOptions(arguments);
        if (toolName == "github_update_status") return updateStatus(arguments);

        return ToolResult::Error("Unknown tool: " + toolName);
    }

private:
    GitHub::ProjectsClient m_client;

    Value issueToValue(const GitHub::Issue& issue) {
        Value v;
        v["key"] = issue.key;
        v["summary"] = issue.summary;
        v["status"] = issue.status;
        v["priority"] = issue.priority;
        v["type"] = issue.type;
        v["assignee"] = issue.assignee;
        v["reporter"] = issue.reporter;
        v["updated"] = issue.updated;
        v["url"] = issue.url;
        if (!issue.description.empty()) {
            v["description"] = issue.description;
        }
        if (!issue.projectItemId.empty()) {
            v["project_item_id"] = issue.projectItemId;
        }
        return v;
    }

    Value commentToValue(const GitHub::Comment& comment) {
        Value v;
        v["id"] = comment.id;
        v["body"] = comment.body;
        v["author"] = comment.author;
        v["created"] = comment.created;
        v["updated"] = comment.updated;
        return v;
    }

    Value statusOptionToValue(const GitHub::StatusOption& opt) {
        Value v;
        v["id"] = opt.id;
        v["name"] = opt.name;
        return v;
    }

    // ========== Tool Implementations ==========

    ToolResult listItems(const Value& args) {
        std::string statusFilter = args.has("status_filter") ? args["status_filter"].asString() : "";
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 30;

        auto result = m_client.ListItems(maxResults, statusFilter);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value items;
        for (const auto& issue : result.data) {
            items.push_back(issueToValue(issue));
        }
        response["items"] = items;
        return ToolResult::Success(response);
    }

    ToolResult getMyItems(const Value& args) {
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 20;

        auto result = m_client.GetMyItems(maxResults);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value items;
        for (const auto& issue : result.data) {
            items.push_back(issueToValue(issue));
        }
        response["items"] = items;
        return ToolResult::Success(response);
    }

    ToolResult getIssue(const Value& args) {
        if (!args.has("issue_ref")) {
            return ToolResult::Error("Missing required parameter: issue_ref");
        }

        std::string issueRef = args["issue_ref"].asString();
        auto result = m_client.GetIssue(issueRef);
        if (!result.success) return ToolResult::Error(result.error);

        return ToolResult::Success(issueToValue(result.data));
    }

    ToolResult createDraft(const Value& args) {
        if (!args.has("title")) {
            return ToolResult::Error("Missing required parameter: title");
        }

        std::string title = args["title"].asString();
        std::string body = args.has("body") ? args["body"].asString() : "";

        auto result = m_client.CreateDraftIssue(title, body);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["item_id"] = result.data;
        response["message"] = "Draft issue created successfully";
        return ToolResult::Success(response);
    }

    ToolResult addComment(const Value& args) {
        if (!args.has("issue_ref") || !args.has("comment")) {
            return ToolResult::Error("Missing required parameters: issue_ref, comment");
        }

        std::string issueRef = args["issue_ref"].asString();
        std::string comment = args["comment"].asString();

        auto result = m_client.AddComment(issueRef, comment);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["id"] = result.data;
        response["message"] = "Comment added successfully";
        return ToolResult::Success(response);
    }

    ToolResult getComments(const Value& args) {
        if (!args.has("issue_ref")) {
            return ToolResult::Error("Missing required parameter: issue_ref");
        }

        std::string issueRef = args["issue_ref"].asString();
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 20;

        auto result = m_client.GetComments(issueRef, maxResults);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value comments;
        for (const auto& c : result.data) {
            comments.push_back(commentToValue(c));
        }
        response["comments"] = comments;
        return ToolResult::Success(response);
    }

    ToolResult getStatusOptions(const Value& /*args*/) {
        auto result = m_client.GetStatusOptions();
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value options;
        for (const auto& opt : result.data) {
            options.push_back(statusOptionToValue(opt));
        }
        response["status_options"] = options;
        return ToolResult::Success(response);
    }

    ToolResult updateStatus(const Value& args) {
        if (!args.has("item_id") || !args.has("status_option_id")) {
            return ToolResult::Error("Missing required parameters: item_id, status_option_id");
        }

        std::string itemId = args["item_id"].asString();
        std::string statusOptionId = args["status_option_id"].asString();

        auto result = m_client.UpdateItemStatus(itemId, statusOptionId);
        if (!result.success) return ToolResult::Error(result.error);

        Value response;
        response["success"] = true;
        response["message"] = "Status updated successfully";
        return ToolResult::Success(response);
    }
};

} // namespace MCP

#endif // MCP_GITHUB_PROJECTS_H
