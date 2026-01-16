#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include "command.h"
#include <map>
#include <vector>
#include <algorithm>

/**
 * Central registry for all commands in the application.
 * Supports registering, unregistering, and querying commands.
 * Designed as a singleton for easy global access.
 */
class CommandRegistry {
public:
    static CommandRegistry& Instance() {
        static CommandRegistry instance;
        return instance;
    }

    // Register a command
    void Register(CommandPtr command) {
        if (command) {
            m_commands[command->GetId()] = command;
        }
    }

    // Register multiple commands at once
    void RegisterAll(const std::vector<CommandPtr>& commands) {
        for (const auto& cmd : commands) {
            Register(cmd);
        }
    }

    // Unregister a command by ID
    void Unregister(const wxString& id) {
        m_commands.erase(id);
    }

    // Get a command by ID
    CommandPtr GetCommand(const wxString& id) const {
        auto it = m_commands.find(id);
        if (it != m_commands.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Get all registered commands
    std::vector<CommandPtr> GetAllCommands() const {
        std::vector<CommandPtr> result;
        result.reserve(m_commands.size());
        for (const auto& pair : m_commands) {
            result.push_back(pair.second);
        }
        return result;
    }

    // Get all commands in a specific category
    std::vector<CommandPtr> GetCommandsByCategory(const wxString& category) const {
        std::vector<CommandPtr> result;
        for (const auto& pair : m_commands) {
            if (pair.second->GetCategory() == category) {
                result.push_back(pair.second);
            }
        }
        return result;
    }

    // Get all unique categories
    std::vector<wxString> GetCategories() const {
        std::vector<wxString> categories;
        for (const auto& pair : m_commands) {
            const wxString& cat = pair.second->GetCategory();
            if (!cat.IsEmpty() && 
                std::find(categories.begin(), categories.end(), cat) == categories.end()) {
                categories.push_back(cat);
            }
        }
        std::sort(categories.begin(), categories.end());
        return categories;
    }

    // Search commands by query string (fuzzy matching on title/category)
    std::vector<CommandPtr> Search(const wxString& query, const CommandContext& context) const {
        std::vector<std::pair<CommandPtr, int>> scored;
        wxString lowerQuery = query.Lower();

        for (const auto& pair : m_commands) {
            const auto& cmd = pair.second;
            
            // Skip disabled commands
            if (!cmd->IsEnabled(context)) {
                continue;
            }

            int score = CalculateMatchScore(cmd, lowerQuery);
            if (score > 0 || query.IsEmpty()) {
                scored.push_back({cmd, score});
            }
        }

        // Sort by score (descending), then alphabetically
        std::sort(scored.begin(), scored.end(), 
            [](const auto& a, const auto& b) {
                if (a.second != b.second) {
                    return a.second > b.second;
                }
                return a.first->GetDisplayString() < b.first->GetDisplayString();
            });

        std::vector<CommandPtr> result;
        result.reserve(scored.size());
        for (const auto& pair : scored) {
            result.push_back(pair.first);
        }
        return result;
    }

    // Execute a command by ID
    bool Execute(const wxString& id, CommandContext& context) {
        auto cmd = GetCommand(id);
        if (cmd && cmd->IsEnabled(context)) {
            cmd->Execute(context);
            return true;
        }
        return false;
    }

    // Clear all commands
    void Clear() {
        m_commands.clear();
    }

private:
    CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;

    // Calculate fuzzy match score
    int CalculateMatchScore(const CommandPtr& cmd, const wxString& query) const {
        if (query.IsEmpty()) {
            return 1;
        }

        wxString title = cmd->GetTitle().Lower();
        wxString category = cmd->GetCategory().Lower();
        wxString display = cmd->GetDisplayString().Lower();
        
        int score = 0;

        // Exact match in title
        if (title == query) {
            score += 100;
        }
        // Title starts with query
        else if (title.StartsWith(query)) {
            score += 80;
        }
        // Title contains query
        else if (title.Contains(query)) {
            score += 60;
        }
        // Category contains query
        else if (category.Contains(query)) {
            score += 40;
        }
        // Fuzzy match - all query chars appear in order
        else if (FuzzyMatch(display, query)) {
            score += 20;
        }

        return score;
    }

    // Simple fuzzy matching - checks if all chars in query appear in text in order
    bool FuzzyMatch(const wxString& text, const wxString& query) const {
        size_t textIdx = 0;
        for (size_t queryIdx = 0; queryIdx < query.Length(); ++queryIdx) {
            bool found = false;
            while (textIdx < text.Length()) {
                if (text[textIdx] == query[queryIdx]) {
                    found = true;
                    ++textIdx;
                    break;
                }
                ++textIdx;
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }

    std::map<wxString, CommandPtr> m_commands;
};

#endif // COMMAND_REGISTRY_H
