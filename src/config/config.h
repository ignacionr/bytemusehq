#ifndef CONFIG_H
#define CONFIG_H

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/file.h>
#include <wx/textfile.h>
#include <map>
#include <vector>
#include <functional>
#include <variant>
#include <optional>

/**
 * Configuration value types supported by the config system.
 * Extensible to support various data types for different use cases.
 */
using ConfigValue = std::variant<
    bool,
    int,
    double,
    wxString,
    std::vector<wxString>
>;

/**
 * Listener callback for configuration changes.
 * Called when a specific key or any key in a namespace changes.
 */
using ConfigChangeListener = std::function<void(const wxString& key, const ConfigValue& newValue)>;

/**
 * Central configuration manager for ByteMuseHQ.
 * 
 * Features:
 * - JSON-based configuration stored in ~/.bytemusehq/config.json
 * - Hierarchical keys using dot notation (e.g., "editor.theme", "terminal.fontSize")
 * - Type-safe value access with defaults
 * - Change notification system for reactive updates
 * - Designed for extensibility (extensions can register their own namespaces)
 * 
 * Usage:
 *   auto& config = Config::Instance();
 *   wxString theme = config.GetString("theme.current", "dark");
 *   config.Set("theme.current", wxString("light"));
 *   config.Save();
 */
class Config {
public:
    static Config& Instance() {
        static Config instance;
        return instance;
    }

    // Prevent copying
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // ========== File Operations ==========
    
    /**
     * Load configuration from the default config file.
     * Creates default config if file doesn't exist.
     */
    bool Load();
    
    /**
     * Save configuration to the default config file.
     */
    bool Save();
    
    /**
     * Get the config directory path (~/.bytemusehq/)
     */
    wxString GetConfigDir() const;
    
    /**
     * Get the config file path (~/.bytemusehq/config.json)
     */
    wxString GetConfigFilePath() const;

    // ========== Value Getters ==========
    
    /**
     * Get a string value with optional default.
     */
    wxString GetString(const wxString& key, const wxString& defaultValue = "") const;
    
    /**
     * Get an integer value with optional default.
     */
    int GetInt(const wxString& key, int defaultValue = 0) const;
    
    /**
     * Get a double value with optional default.
     */
    double GetDouble(const wxString& key, double defaultValue = 0.0) const;
    
    /**
     * Get a boolean value with optional default.
     */
    bool GetBool(const wxString& key, bool defaultValue = false) const;
    
    /**
     * Get a string array value with optional default.
     */
    std::vector<wxString> GetStringArray(const wxString& key, 
                                         const std::vector<wxString>& defaultValue = {}) const;
    
    /**
     * Check if a key exists in the configuration.
     */
    bool HasKey(const wxString& key) const;
    
    /**
     * Get the raw ConfigValue, if it exists.
     */
    std::optional<ConfigValue> Get(const wxString& key) const;

    // ========== Value Setters ==========
    
    /**
     * Set a configuration value.
     * Notifies all registered listeners for this key.
     */
    void Set(const wxString& key, const ConfigValue& value);
    
    /**
     * Remove a key from configuration.
     */
    void Remove(const wxString& key);
    
    /**
     * Clear all configuration.
     */
    void Clear();

    // ========== Change Notification ==========
    
    /**
     * Add a listener for changes to a specific key.
     * Returns an ID that can be used to remove the listener.
     */
    int AddListener(const wxString& key, ConfigChangeListener listener);
    
    /**
     * Add a listener for changes to any key in a namespace.
     * Example: AddNamespaceListener("editor", callback) listens to "editor.*"
     */
    int AddNamespaceListener(const wxString& ns, ConfigChangeListener listener);
    
    /**
     * Remove a listener by its ID.
     */
    void RemoveListener(int listenerId);

    // ========== Utility ==========
    
    /**
     * Get all keys that start with a given prefix.
     */
    std::vector<wxString> GetKeysWithPrefix(const wxString& prefix) const;
    
    /**
     * Set default values if keys don't exist.
     * Useful for extensions to register their defaults.
     */
    void SetDefaults(const std::map<wxString, ConfigValue>& defaults);

private:
    Config();
    ~Config() = default;

    // Configuration storage
    std::map<wxString, ConfigValue> m_values;
    
    // Listeners
    struct Listener {
        int id;
        wxString pattern;  // Key or namespace pattern
        bool isNamespace;
        ConfigChangeListener callback;
    };
    std::vector<Listener> m_listeners;
    int m_nextListenerId = 1;
    
    // Notify listeners of a change
    void NotifyListeners(const wxString& key, const ConfigValue& value);
    
    // JSON serialization helpers
    wxString SerializeToJson() const;
    bool ParseFromJson(const wxString& json);
    
    // Ensure config directory exists
    bool EnsureConfigDir();
    
    // Initialize with default values
    void InitializeDefaults();
};

#endif // CONFIG_H
