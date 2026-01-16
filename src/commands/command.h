#ifndef COMMAND_H
#define COMMAND_H

#include <wx/wx.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>

// Forward declaration
class CommandContext;

/**
 * Base class for all commands in the system.
 * Commands encapsulate actions that can be executed via the command palette,
 * keyboard shortcuts, or menu items.
 */
class Command {
public:
    using ExecuteFunc = std::function<void(CommandContext&)>;
    using EnabledFunc = std::function<bool(const CommandContext&)>;

    Command(const wxString& id, const wxString& title, const wxString& category = "")
        : m_id(id)
        , m_title(title)
        , m_category(category)
        , m_description("")
        , m_shortcut("")
    {}

    virtual ~Command() = default;

    // Core properties
    const wxString& GetId() const { return m_id; }
    const wxString& GetTitle() const { return m_title; }
    const wxString& GetCategory() const { return m_category; }
    const wxString& GetDescription() const { return m_description; }
    const wxString& GetShortcut() const { return m_shortcut; }

    // Fluent setters for building commands
    Command& SetDescription(const wxString& desc) { m_description = desc; return *this; }
    Command& SetShortcut(const wxString& shortcut) { m_shortcut = shortcut; return *this; }
    Command& SetExecuteHandler(ExecuteFunc func) { m_executeFunc = std::move(func); return *this; }
    Command& SetEnabledHandler(EnabledFunc func) { m_enabledFunc = std::move(func); return *this; }

    // Display string for the command palette
    wxString GetDisplayString() const {
        if (m_category.IsEmpty()) {
            return m_title;
        }
        return m_category + ": " + m_title;
    }

    // Check if command is currently enabled
    virtual bool IsEnabled(const CommandContext& context) const {
        if (m_enabledFunc) {
            return m_enabledFunc(context);
        }
        return true;
    }

    // Execute the command
    virtual void Execute(CommandContext& context) {
        if (m_executeFunc) {
            m_executeFunc(context);
        }
    }

protected:
    wxString m_id;
    wxString m_title;
    wxString m_category;
    wxString m_description;
    wxString m_shortcut;
    ExecuteFunc m_executeFunc;
    EnabledFunc m_enabledFunc;
};

/**
 * Context object passed to commands during execution.
 * Provides access to the application state and services.
 */
class CommandContext {
public:
    CommandContext() = default;

    // Store arbitrary data in the context
    template<typename T>
    void Set(const wxString& key, T* value) {
        m_data[key] = static_cast<void*>(value);
    }

    template<typename T>
    T* Get(const wxString& key) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

    bool Has(const wxString& key) const {
        return m_data.find(key) != m_data.end();
    }

private:
    std::map<wxString, void*> m_data;
};

// Convenience type alias
using CommandPtr = std::shared_ptr<Command>;

#endif // COMMAND_H
