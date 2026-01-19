#ifndef WIDGET_H
#define WIDGET_H

#include <wx/wx.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

// Forward declarations
class Widget;
class WidgetRegistry;
class WidgetContext;
class CommandRegistry;

/**
 * Shared pointer type for widgets
 */
using WidgetPtr = std::shared_ptr<Widget>;

/**
 * Widget placement locations within the main frame.
 */
enum class WidgetLocation {
    Sidebar,        // Left panel (like file tree)
    Editor,         // Main editor area (center)
    Panel,          // Bottom panel (like terminal)
    StatusBar,      // Status bar area
    ToolBar         // Tool bar area
};

/**
 * Widget metadata describing the widget's properties and behavior.
 */
struct WidgetInfo {
    wxString id;                    // Unique identifier (e.g., "core.fileTree")
    wxString name;                  // Display name
    wxString description;           // Brief description
    WidgetLocation location;        // Where the widget should be placed
    int priority;                   // Display order (higher = shown first)
    bool showByDefault;             // Whether to show on startup
    
    WidgetInfo() : location(WidgetLocation::Panel), priority(0), showByDefault(true) {}
};

/**
 * Context object providing access to application services for widgets.
 */
class WidgetContext {
public:
    WidgetContext() = default;

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

/**
 * Abstract base class for all visual widgets in ByteMuseHQ.
 * 
 * Widgets are visual components that can be registered and managed
 * by the WidgetRegistry. They provide a consistent interface for:
 * - Creation and lifecycle management
 * - Theme support
 * - Focus handling
 * - Visibility control
 * 
 * To create a custom widget:
 * 1. Inherit from Widget
 * 2. Implement GetInfo() to return widget metadata
 * 3. Implement CreateWindow() to build the UI
 * 4. Optionally override OnThemeChanged(), OnFocus(), etc.
 * 5. Register with WidgetRegistry::Instance().Register(...)
 * 
 * Example:
 * @code
 * class MyWidget : public Widget {
 * public:
 *     WidgetInfo GetInfo() const override {
 *         WidgetInfo info;
 *         info.id = "myextension.mywidget";
 *         info.name = "My Widget";
 *         info.location = WidgetLocation::Panel;
 *         return info;
 *     }
 *     
 *     wxWindow* CreateWindow(wxWindow* parent, WidgetContext& ctx) override {
 *         auto* panel = new wxPanel(parent);
 *         // Build your UI here
 *         return panel;
 *     }
 * };
 * @endcode
 */
class Widget {
public:
    virtual ~Widget() = default;

    /**
     * Get metadata about this widget.
     * @return WidgetInfo structure with widget properties
     */
    virtual WidgetInfo GetInfo() const = 0;

    /**
     * Create the widget's window.
     * Called once when the widget is first shown.
     * 
     * @param parent The parent window
     * @param context Context providing access to application services
     * @return The created window (ownership transferred to parent)
     */
    virtual wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) = 0;

    /**
     * Called when the application theme changes.
     * Override to update colors and styles.
     * 
     * @param window The widget's window (as returned by CreateWindow)
     * @param context Current context
     */
    virtual void OnThemeChanged(wxWindow* window, WidgetContext& context) {}

    /**
     * Called when the widget receives focus.
     * Override to handle focus-related logic.
     * 
     * @param window The widget's window
     * @param context Current context
     */
    virtual void OnFocus(wxWindow* window, WidgetContext& context) {}

    /**
     * Called when the widget is about to be shown.
     * Override to perform any necessary setup.
     * 
     * @param window The widget's window
     * @param context Current context
     */
    virtual void OnShow(wxWindow* window, WidgetContext& context) {}

    /**
     * Called when the widget is about to be hidden.
     * Override to perform any necessary cleanup.
     * 
     * @param window The widget's window
     * @param context Current context
     */
    virtual void OnHide(wxWindow* window, WidgetContext& context) {}

    /**
     * Get commands specific to this widget.
     * Override to provide widget-specific commands for the command palette.
     * 
     * @return Vector of command IDs that this widget provides
     */
    virtual std::vector<wxString> GetCommands() const { return {}; }

    /**
     * Register commands provided by this widget.
     * Called when the widget is first instantiated.
     * Override to register widget-specific commands with the CommandRegistry.
     * 
     * @param context The widget context providing access to app services
     */
    virtual void RegisterCommands(WidgetContext& context) {}
};

/**
 * Factory function type for creating widgets.
 */
using WidgetFactory = std::function<WidgetPtr()>;

/**
 * Central registry for all widgets in the application.
 * 
 * The WidgetRegistry is a singleton that manages:
 * - Widget registration and unregistration
 * - Widget lookup by ID or location
 * - Widget lifecycle management
 * 
 * Built-in widgets (Editor, FileTree, Terminal) are registered
 * automatically. Extensions can register additional widgets.
 * 
 * Example usage:
 * @code
 * // Register a widget factory
 * WidgetRegistry::Instance().Register("mywidget", []() {
 *     return std::make_shared<MyWidget>();
 * });
 * 
 * // Get all widgets for a location
 * auto panelWidgets = WidgetRegistry::Instance()
 *     .GetWidgetsByLocation(WidgetLocation::Panel);
 * @endcode
 */
class WidgetRegistry {
public:
    /**
     * Get the singleton instance.
     */
    static WidgetRegistry& Instance() {
        static WidgetRegistry instance;
        return instance;
    }

    /**
     * Register a widget factory.
     * 
     * @param id Unique identifier for the widget
     * @param factory Function that creates widget instances
     */
    void Register(const wxString& id, WidgetFactory factory) {
        m_factories[id] = std::move(factory);
    }

    /**
     * Register a widget instance directly.
     * 
     * @param widget The widget to register
     */
    void Register(WidgetPtr widget) {
        if (widget) {
            m_widgets[widget->GetInfo().id] = widget;
        }
    }

    /**
     * Unregister a widget by ID.
     * 
     * @param id The widget ID to unregister
     */
    void Unregister(const wxString& id) {
        m_factories.erase(id);
        m_widgets.erase(id);
    }

    /**
     * Get a widget by ID.
     * Creates the widget using its factory if not already instantiated.
     * 
     * @param id The widget ID
     * @return Shared pointer to the widget, or nullptr if not found
     */
    WidgetPtr GetWidget(const wxString& id) {
        // Check if already instantiated
        auto it = m_widgets.find(id);
        if (it != m_widgets.end()) {
            return it->second;
        }

        // Try to create from factory
        auto factoryIt = m_factories.find(id);
        if (factoryIt != m_factories.end()) {
            auto widget = factoryIt->second();
            m_widgets[id] = widget;
            return widget;
        }

        return nullptr;
    }

    /**
     * Get all registered widget IDs.
     * 
     * @return Vector of widget IDs
     */
    std::vector<wxString> GetAllIds() const {
        std::vector<wxString> ids;
        for (const auto& pair : m_factories) {
            ids.push_back(pair.first);
        }
        for (const auto& pair : m_widgets) {
            if (m_factories.find(pair.first) == m_factories.end()) {
                ids.push_back(pair.first);
            }
        }
        return ids;
    }

    /**
     * Get all widgets for a specific location.
     * 
     * @param location The widget location
     * @return Vector of widgets at that location, sorted by priority
     */
    std::vector<WidgetPtr> GetWidgetsByLocation(WidgetLocation location) {
        std::vector<WidgetPtr> result;
        
        // Gather all widgets (instantiating from factories as needed)
        for (const auto& id : GetAllIds()) {
            auto widget = GetWidget(id);
            if (widget && widget->GetInfo().location == location) {
                result.push_back(widget);
            }
        }
        
        // Sort by priority (descending)
        std::sort(result.begin(), result.end(),
            [](const WidgetPtr& a, const WidgetPtr& b) {
                return a->GetInfo().priority > b->GetInfo().priority;
            });
        
        return result;
    }

    /**
     * Get all instantiated widgets.
     * 
     * @return Vector of all widget instances
     */
    std::vector<WidgetPtr> GetAllWidgets() const {
        std::vector<WidgetPtr> result;
        for (const auto& pair : m_widgets) {
            result.push_back(pair.second);
        }
        return result;
    }

private:
    WidgetRegistry() = default;
    WidgetRegistry(const WidgetRegistry&) = delete;
    WidgetRegistry& operator=(const WidgetRegistry&) = delete;

    std::map<wxString, WidgetFactory> m_factories;
    std::map<wxString, WidgetPtr> m_widgets;
};

#endif // WIDGET_H
