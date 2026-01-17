#ifndef THEME_H
#define THEME_H

#include <wx/wx.h>
#include <wx/colour.h>
#include <map>
#include <memory>
#include <vector>

/**
 * Represents a complete visual theme for ByteMuseHQ.
 * Contains all colors and style settings needed to render the UI.
 */
class Theme {
public:
    wxString id;           // Unique identifier (e.g., "dark", "light")
    wxString name;         // Display name (e.g., "Dark Theme")
    wxString description;  // Optional description
    bool isDark;           // Whether this is a dark theme (for system integration)

    // ========== Editor Colors ==========
    struct EditorColors {
        wxColour background;
        wxColour foreground;
        wxColour lineNumberBackground;
        wxColour lineNumberForeground;
        wxColour caretLine;
        wxColour caret;
        wxColour selection;
        wxColour selectionForeground;
        wxColour whitespace;
        wxColour indentGuide;
        
        // Syntax highlighting
        wxColour comment;
        wxColour keyword;
        wxColour string;
        wxColour number;
        wxColour operator_;
        wxColour preprocessor;
        wxColour identifier;
        wxColour type;       // For type names
        wxColour function;   // For function names
    } editor;
    
    // ========== Terminal Colors ==========
    struct TerminalColors {
        wxColour background;
        wxColour foreground;
        wxColour inputBackground;
        wxColour inputForeground;
        wxColour prompt;
        wxColour error;
    } terminal;
    
    // ========== UI Colors ==========
    struct UIColors {
        wxColour windowBackground;
        wxColour panelBackground;
        wxColour sidebarBackground;
        wxColour sidebarForeground;
        wxColour sidebarSelection;
        wxColour statusBarBackground;
        wxColour statusBarForeground;
        wxColour titleBarBackground;
        wxColour titleBarForeground;
        wxColour border;
        wxColour separator;
        wxColour scrollbar;
        wxColour scrollbarHover;
    } ui;
    
    // ========== Dialog/Palette Colors ==========
    struct PaletteColors {
        wxColour background;
        wxColour foreground;
        wxColour inputBackground;
        wxColour inputForeground;
        wxColour itemHover;
        wxColour itemSelected;
        wxColour border;
        wxColour shadow;
        wxColour categoryForeground;
        wxColour shortcutForeground;
    } palette;
};

using ThemePtr = std::shared_ptr<Theme>;

/**
 * Manages themes for the application.
 * 
 * Features:
 * - Built-in themes (dark, light)
 * - Theme registration system for custom/extension themes
 * - Theme switching with notification
 * - Integration with Config system
 */
class ThemeManager {
public:
    using ThemeChangeCallback = std::function<void(const ThemePtr& newTheme)>;

    static ThemeManager& Instance() {
        static ThemeManager instance;
        return instance;
    }
    
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    /**
     * Initialize theme manager and load current theme from config.
     */
    void Initialize();
    
    /**
     * Get the currently active theme.
     */
    ThemePtr GetCurrentTheme() const { return m_currentTheme; }
    
    /**
     * Get a theme by ID.
     */
    ThemePtr GetTheme(const wxString& id) const;
    
    /**
     * Get all available themes.
     */
    std::vector<ThemePtr> GetAllThemes() const;
    
    /**
     * Get list of theme IDs.
     */
    std::vector<wxString> GetThemeIds() const;
    
    /**
     * Set the current theme by ID.
     * Saves to config and notifies listeners.
     */
    bool SetCurrentTheme(const wxString& id);
    
    /**
     * Register a custom theme.
     */
    void RegisterTheme(ThemePtr theme);
    
    /**
     * Unregister a theme by ID.
     */
    void UnregisterTheme(const wxString& id);
    
    /**
     * Add a listener for theme changes.
     * Returns ID for removal.
     */
    int AddChangeListener(ThemeChangeCallback callback);
    
    /**
     * Remove a listener.
     */
    void RemoveChangeListener(int id);

private:
    ThemeManager();
    ~ThemeManager() = default;
    
    void CreateBuiltinThemes();
    ThemePtr CreateDarkTheme();
    ThemePtr CreateLightTheme();
    
    void NotifyListeners();
    
    std::map<wxString, ThemePtr> m_themes;
    ThemePtr m_currentTheme;
    
    struct Listener {
        int id;
        ThemeChangeCallback callback;
    };
    std::vector<Listener> m_listeners;
    int m_nextListenerId = 1;
};

#endif // THEME_H
