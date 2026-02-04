#ifndef MCP_JIRA_H
#define MCP_JIRA_H

#include "mcp.h"
#include "../jira/jira_client.h"
#include <wx/log.h>

namespace MCP {

/**
 * Jira MCP Provider.
 * 
 * Provides tools for the AI to interact with Jira issue tracking.
 * This enables the AI to search, view, create, and manage Jira issues.
 * 
 * Available tools:
 * - jira_search_issues: Search for issues using JQL
 * - jira_get_my_issues: Get issues assigned to current user
 * - jira_get_issue: Get details of a specific issue
 * - jira_create_issue: Create a new issue
 * - jira_add_comment: Add a comment to an issue
 * - jira_get_comments: Get comments on an issue
 * - jira_get_transitions: Get available status transitions
 * - jira_transition_issue: Transition an issue to a new status
 */
class JiraProvider : public Provider {
public:
    JiraProvider() : m_client(Jira::ClientConfig::LoadFromConfig()) {
        auto cfg = m_client.GetConfig();
        wxLogDebug("MCP Jira: Initialized with apiUrl='%s', user='%s', configured=%s",
                   cfg.apiUrl.c_str(), cfg.user.c_str(),
                   m_client.IsConfigured() ? "yes" : "no");
    }
    
    explicit JiraProvider(const Jira::ClientConfig& config) : m_client(config) {}
    
    std::string getId() const override {
        return "mcp.jira";
    }
    
    std::string getName() const override {
        return "Jira";
    }
    
    std::string getDescription() const override {
        return "Provides access to Jira issue tracking";
    }
    
    /**
     * Update the Jira client configuration.
     */
    void setConfig(const Jira::ClientConfig& config) {
        m_client.SetConfig(config);
    }
    
    /**
     * Reload configuration from Config singleton.
     */
    void reloadConfig() {
        m_client.SetConfig(Jira::ClientConfig::LoadFromConfig());
    }
    
    /**
     * Check if Jira is configured.
     */
    bool isConfigured() const {
        return m_client.IsConfigured();
    }
    
    bool isEnabled() const override {
        return m_enabled && m_client.IsConfigured();
    }
    
    std::vector<ToolDefinition> getTools() const override {
        std::vector<ToolDefinition> tools;
        
        // jira_search_issues
        {
            ToolDefinition tool;
            tool.name = "jira_search_issues";
            tool.description = "Search for Jira issues using JQL (Jira Query Language). "
                             "Examples: 'project=PROJ', 'assignee=currentUser()', "
                             "'status=\"In Progress\"', 'labels=bug AND created>=-7d'.";
            tool.parameters = {
                {"jql", "string", "JQL query to search for issues", true},
                {"max_results", "number", "Maximum number of results to return (default: 20)", false}
            };
            tools.push_back(tool);
        }
        
        // jira_get_my_issues
        {
            ToolDefinition tool;
            tool.name = "jira_get_my_issues";
            tool.description = "Get issues assigned to the current user, sorted by last update.";
            tool.parameters = {
                {"max_results", "number", "Maximum number of results to return (default: 20)", false}
            };
            tools.push_back(tool);
        }
        
        // jira_get_issue
        {
            ToolDefinition tool;
            tool.name = "jira_get_issue";
            tool.description = "Get detailed information about a specific Jira issue.";
            tool.parameters = {
                {"issue_key", "string", "The issue key (e.g., 'PROJ-123')", true}
            };
            tools.push_back(tool);
        }
        
        // jira_create_issue
        {
            ToolDefinition tool;
            tool.name = "jira_create_issue";
            tool.description = "Create a new Jira issue.";
            tool.parameters = {
                {"project", "string", "Project key (e.g., 'PROJ')", true},
                {"summary", "string", "Issue title/summary", true},
                {"issue_type", "string", "Issue type: 'Task', 'Bug', 'Story', 'Epic', 'Sub-task'", true},
                {"description", "string", "Issue description (optional)", false},
                {"priority", "string", "Priority: 'Highest', 'High', 'Medium', 'Low', 'Lowest' (optional)", false}
            };
            tools.push_back(tool);
        }
        
        // jira_add_comment
        {
            ToolDefinition tool;
            tool.name = "jira_add_comment";
            tool.description = "Add a comment to a Jira issue.";
            tool.parameters = {
                {"issue_key", "string", "The issue key (e.g., 'PROJ-123')", true},
                {"comment", "string", "The comment text to add", true}
            };
            tools.push_back(tool);
        }
        
        // jira_get_comments
        {
            ToolDefinition tool;
            tool.name = "jira_get_comments";
            tool.description = "Get comments on a Jira issue.";
            tool.parameters = {
                {"issue_key", "string", "The issue key (e.g., 'PROJ-123')", true},
                {"max_results", "number", "Maximum number of comments to return (default: 20)", false}
            };
            tools.push_back(tool);
        }
        
        // jira_get_transitions
        {
            ToolDefinition tool;
            tool.name = "jira_get_transitions";
            tool.description = "Get available status transitions for a Jira issue. "
                             "Use this to see what statuses an issue can be moved to.";
            tool.parameters = {
                {"issue_key", "string", "The issue key (e.g., 'PROJ-123')", true}
            };
            tools.push_back(tool);
        }
        
        // jira_transition_issue
        {
            ToolDefinition tool;
            tool.name = "jira_transition_issue";
            tool.description = "Transition a Jira issue to a new status. "
                             "Use jira_get_transitions first to get available transition IDs.";
            tool.parameters = {
                {"issue_key", "string", "The issue key (e.g., 'PROJ-123')", true},
                {"transition_id", "string", "The transition ID to execute", true}
            };
            tools.push_back(tool);
        }
        
        return tools;
    }
    
    ToolResult executeTool(const std::string& toolName, const Value& arguments) override {
        if (!m_client.IsConfigured()) {
            return ToolResult::Error("Jira is not configured. Please set jira.apiUrl, jira.user, and jira.apiToken in settings.");
        }
        
        if (toolName == "jira_search_issues") {
            return searchIssues(arguments);
        } else if (toolName == "jira_get_my_issues") {
            return getMyIssues(arguments);
        } else if (toolName == "jira_get_issue") {
            return getIssue(arguments);
        } else if (toolName == "jira_create_issue") {
            return createIssue(arguments);
        } else if (toolName == "jira_add_comment") {
            return addComment(arguments);
        } else if (toolName == "jira_get_comments") {
            return getComments(arguments);
        } else if (toolName == "jira_get_transitions") {
            return getTransitions(arguments);
        } else if (toolName == "jira_transition_issue") {
            return transitionIssue(arguments);
        }
        
        return ToolResult::Error("Unknown tool: " + toolName);
    }

private:
    Jira::Client m_client;
    
    /**
     * Convert Issue to MCP Value.
     */
    Value issueToValue(const Jira::Issue& issue) {
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
        return v;
    }
    
    /**
     * Convert Comment to MCP Value.
     */
    Value commentToValue(const Jira::Comment& comment) {
        Value v;
        v["id"] = comment.id;
        v["body"] = comment.body;
        v["author"] = comment.author;
        v["created"] = comment.created;
        v["updated"] = comment.updated;
        return v;
    }
    
    /**
     * Convert Transition to MCP Value.
     */
    Value transitionToValue(const Jira::Transition& transition) {
        Value v;
        v["id"] = transition.id;
        v["name"] = transition.name;
        v["to_status"] = transition.toStatus;
        return v;
    }
    
    // ========== Tool Implementations ==========
    
    ToolResult searchIssues(const Value& args) {
        if (!args.has("jql")) {
            return ToolResult::Error("Missing required parameter: jql");
        }
        
        std::string jql = args["jql"].asString();
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 20;
        
        auto result = m_client.SearchIssues(jql, maxResults);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value issues;
        for (const auto& issue : result.data) {
            issues.push_back(issueToValue(issue));
        }
        response["issues"] = issues;
        
        return ToolResult::Success(response);
    }
    
    ToolResult getMyIssues(const Value& args) {
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 20;
        
        auto result = m_client.GetMyIssues(maxResults);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value issues;
        for (const auto& issue : result.data) {
            issues.push_back(issueToValue(issue));
        }
        response["issues"] = issues;
        
        return ToolResult::Success(response);
    }
    
    ToolResult getIssue(const Value& args) {
        if (!args.has("issue_key")) {
            return ToolResult::Error("Missing required parameter: issue_key");
        }
        
        std::string issueKey = args["issue_key"].asString();
        
        auto result = m_client.GetIssue(issueKey);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        return ToolResult::Success(issueToValue(result.data));
    }
    
    ToolResult createIssue(const Value& args) {
        if (!args.has("project") || !args.has("summary") || !args.has("issue_type")) {
            return ToolResult::Error("Missing required parameters: project, summary, issue_type");
        }
        
        std::string project = args["project"].asString();
        std::string summary = args["summary"].asString();
        std::string issueType = args["issue_type"].asString();
        std::string description = args.has("description") ? args["description"].asString() : "";
        std::string priority = args.has("priority") ? args["priority"].asString() : "";
        
        auto result = m_client.CreateIssue(project, summary, issueType, description, priority);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["key"] = result.data;
        response["url"] = m_client.GetConfig().apiUrl + "/browse/" + result.data;
        response["message"] = "Issue created successfully";
        
        return ToolResult::Success(response);
    }
    
    ToolResult addComment(const Value& args) {
        if (!args.has("issue_key") || !args.has("comment")) {
            return ToolResult::Error("Missing required parameters: issue_key, comment");
        }
        
        std::string issueKey = args["issue_key"].asString();
        std::string comment = args["comment"].asString();
        
        auto result = m_client.AddComment(issueKey, comment);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["id"] = result.data;
        response["message"] = "Comment added successfully";
        
        return ToolResult::Success(response);
    }
    
    ToolResult getComments(const Value& args) {
        if (!args.has("issue_key")) {
            return ToolResult::Error("Missing required parameter: issue_key");
        }
        
        std::string issueKey = args["issue_key"].asString();
        int maxResults = args.has("max_results") ? args["max_results"].asInt() : 20;
        
        auto result = m_client.GetComments(issueKey, maxResults);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value comments;
        for (const auto& comment : result.data) {
            comments.push_back(commentToValue(comment));
        }
        response["comments"] = comments;
        
        return ToolResult::Success(response);
    }
    
    ToolResult getTransitions(const Value& args) {
        if (!args.has("issue_key")) {
            return ToolResult::Error("Missing required parameter: issue_key");
        }
        
        std::string issueKey = args["issue_key"].asString();
        
        auto result = m_client.GetTransitions(issueKey);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["total"] = static_cast<int>(result.data.size());
        Value transitions;
        for (const auto& transition : result.data) {
            transitions.push_back(transitionToValue(transition));
        }
        response["transitions"] = transitions;
        
        return ToolResult::Success(response);
    }
    
    ToolResult transitionIssue(const Value& args) {
        if (!args.has("issue_key") || !args.has("transition_id")) {
            return ToolResult::Error("Missing required parameters: issue_key, transition_id");
        }
        
        std::string issueKey = args["issue_key"].asString();
        std::string transitionId = args["transition_id"].asString();
        
        auto result = m_client.TransitionIssue(issueKey, transitionId);
        if (!result.success) {
            return ToolResult::Error(result.error);
        }
        
        Value response;
        response["success"] = true;
        response["message"] = "Issue transitioned successfully";
        
        return ToolResult::Success(response);
    }
};

} // namespace MCP

#endif // MCP_JIRA_H
