#include "config.h"
#include <wx/dir.h>
#include <wx/tokenzr.h>
#include <sstream>
#include <iomanip>

Config::Config() {
    InitializeDefaults();
}

void Config::InitializeDefaults() {
    // Theme defaults
    m_values["theme.current"] = wxString("dark");
    
    // Editor defaults
    m_values["editor.fontSize"] = 12;
    m_values["editor.fontFamily"] = wxString("Menlo");
    m_values["editor.tabSize"] = 4;
    m_values["editor.useTabs"] = false;
    m_values["editor.wordWrap"] = false;
    m_values["editor.showLineNumbers"] = true;
    
    // Terminal defaults
    m_values["terminal.fontSize"] = 12;
    m_values["terminal.fontFamily"] = wxString("Menlo");
    
    // UI defaults
    m_values["ui.sidebarWidth"] = 250;
    m_values["ui.terminalHeight"] = 200;
}

wxString Config::GetConfigDir() const {
    wxString homeDir = wxGetHomeDir();
    return wxFileName(homeDir, ".bytemusehq").GetFullPath();
}

wxString Config::GetConfigFilePath() const {
    return wxFileName(GetConfigDir(), "config.json").GetFullPath();
}

bool Config::EnsureConfigDir() {
    wxString configDir = GetConfigDir();
    if (!wxDir::Exists(configDir)) {
        return wxFileName::Mkdir(configDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    return true;
}

bool Config::Load() {
    wxString configPath = GetConfigFilePath();
    
    if (!wxFileExists(configPath)) {
        // No config file yet, use defaults and create one
        return Save();
    }
    
    wxFile file(configPath, wxFile::read);
    if (!file.IsOpened()) {
        return false;
    }
    
    wxString content;
    if (!file.ReadAll(&content)) {
        return false;
    }
    
    return ParseFromJson(content);
}

bool Config::Save() {
    if (!EnsureConfigDir()) {
        return false;
    }
    
    wxString configPath = GetConfigFilePath();
    wxFile file(configPath, wxFile::write);
    if (!file.IsOpened()) {
        return false;
    }
    
    wxString json = SerializeToJson();
    return file.Write(json);
}

// ========== Value Getters ==========

wxString Config::GetString(const wxString& key, const wxString& defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (auto* str = std::get_if<wxString>(&it->second)) {
            return *str;
        }
    }
    return defaultValue;
}

int Config::GetInt(const wxString& key, int defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (auto* val = std::get_if<int>(&it->second)) {
            return *val;
        }
        if (auto* val = std::get_if<double>(&it->second)) {
            return static_cast<int>(*val);
        }
    }
    return defaultValue;
}

double Config::GetDouble(const wxString& key, double defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (auto* val = std::get_if<double>(&it->second)) {
            return *val;
        }
        if (auto* val = std::get_if<int>(&it->second)) {
            return static_cast<double>(*val);
        }
    }
    return defaultValue;
}

bool Config::GetBool(const wxString& key, bool defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (auto* val = std::get_if<bool>(&it->second)) {
            return *val;
        }
    }
    return defaultValue;
}

std::vector<wxString> Config::GetStringArray(const wxString& key, 
                                              const std::vector<wxString>& defaultValue) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (auto* arr = std::get_if<std::vector<wxString>>(&it->second)) {
            return *arr;
        }
    }
    return defaultValue;
}

bool Config::HasKey(const wxString& key) const {
    return m_values.find(key) != m_values.end();
}

std::optional<ConfigValue> Config::Get(const wxString& key) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ========== Value Setters ==========

void Config::Set(const wxString& key, const ConfigValue& value) {
    m_values[key] = value;
    NotifyListeners(key, value);
}

void Config::Remove(const wxString& key) {
    m_values.erase(key);
}

void Config::Clear() {
    m_values.clear();
    InitializeDefaults();
}

// ========== Change Notification ==========

int Config::AddListener(const wxString& key, ConfigChangeListener listener) {
    int id = m_nextListenerId++;
    m_listeners.push_back({id, key, false, std::move(listener)});
    return id;
}

int Config::AddNamespaceListener(const wxString& ns, ConfigChangeListener listener) {
    int id = m_nextListenerId++;
    m_listeners.push_back({id, ns, true, std::move(listener)});
    return id;
}

void Config::RemoveListener(int listenerId) {
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
            [listenerId](const Listener& l) { return l.id == listenerId; }),
        m_listeners.end());
}

void Config::NotifyListeners(const wxString& key, const ConfigValue& value) {
    for (const auto& listener : m_listeners) {
        bool shouldNotify = false;
        
        if (listener.isNamespace) {
            // Check if key starts with namespace
            shouldNotify = key.StartsWith(listener.pattern + ".");
        } else {
            // Exact key match
            shouldNotify = (key == listener.pattern);
        }
        
        if (shouldNotify && listener.callback) {
            listener.callback(key, value);
        }
    }
}

// ========== Utility ==========

std::vector<wxString> Config::GetKeysWithPrefix(const wxString& prefix) const {
    std::vector<wxString> result;
    for (const auto& pair : m_values) {
        if (pair.first.StartsWith(prefix)) {
            result.push_back(pair.first);
        }
    }
    return result;
}

void Config::SetDefaults(const std::map<wxString, ConfigValue>& defaults) {
    for (const auto& pair : defaults) {
        if (m_values.find(pair.first) == m_values.end()) {
            m_values[pair.first] = pair.second;
        }
    }
}

// ========== JSON Serialization ==========

// Helper to escape JSON strings
static wxString EscapeJsonString(const wxString& str) {
    wxString result;
    for (wxUniChar ch : str) {
        switch (ch.IsAscii() ? static_cast<char>(ch) : 0) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                result += ch;
        }
    }
    return result;
}

wxString Config::SerializeToJson() const {
    wxString json = "{\n";
    
    bool first = true;
    for (const auto& pair : m_values) {
        if (!first) {
            json += ",\n";
        }
        first = false;
        
        json += "  \"" + EscapeJsonString(pair.first) + "\": ";
        
        std::visit([&json](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                json += arg ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int>) {
                json += wxString::Format("%d", arg);
            } else if constexpr (std::is_same_v<T, double>) {
                json += wxString::Format("%.6g", arg);
            } else if constexpr (std::is_same_v<T, wxString>) {
                json += "\"" + EscapeJsonString(arg) + "\"";
            } else if constexpr (std::is_same_v<T, std::vector<wxString>>) {
                json += "[";
                bool arrFirst = true;
                for (const auto& item : arg) {
                    if (!arrFirst) json += ", ";
                    arrFirst = false;
                    json += "\"" + EscapeJsonString(item) + "\"";
                }
                json += "]";
            }
        }, pair.second);
    }
    
    json += "\n}\n";
    return json;
}

// Simple JSON parser (handles our subset of JSON)
bool Config::ParseFromJson(const wxString& json) {
    // Skip whitespace
    auto skipWs = [](const wxString& s, size_t& pos) {
        while (pos < s.length() && (s[pos] == ' ' || s[pos] == '\t' || 
               s[pos] == '\n' || s[pos] == '\r')) {
            pos++;
        }
    };
    
    // Parse string
    auto parseString = [](const wxString& s, size_t& pos) -> wxString {
        if (pos >= s.length() || s[pos] != '"') return "";
        pos++;
        wxString result;
        while (pos < s.length() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.length()) {
                pos++;
                char ch = s[pos].IsAscii() ? static_cast<char>(s[pos].GetValue()) : 0;
                switch (ch) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += s[pos];
                }
            } else {
                result += s[pos];
            }
            pos++;
        }
        if (pos < s.length()) pos++; // Skip closing quote
        return result;
    };
    
    size_t pos = 0;
    skipWs(json, pos);
    
    if (pos >= json.length() || json[pos] != '{') {
        return false;
    }
    pos++;
    
    std::map<wxString, ConfigValue> newValues;
    
    while (pos < json.length()) {
        skipWs(json, pos);
        
        if (pos >= json.length()) break;
        if (json[pos] == '}') break;
        if (json[pos] == ',') {
            pos++;
            continue;
        }
        
        // Parse key
        wxString key = parseString(json, pos);
        if (key.IsEmpty()) break;
        
        skipWs(json, pos);
        if (pos >= json.length() || json[pos] != ':') break;
        pos++;
        skipWs(json, pos);
        
        // Parse value
        if (pos >= json.length()) break;
        
        if (json[pos] == '"') {
            // String
            newValues[key] = parseString(json, pos);
        } else if (json[pos] == '[') {
            // Array of strings
            pos++;
            std::vector<wxString> arr;
            while (pos < json.length() && json[pos] != ']') {
                skipWs(json, pos);
                if (json[pos] == ',') {
                    pos++;
                    continue;
                }
                if (json[pos] == '"') {
                    arr.push_back(parseString(json, pos));
                } else if (json[pos] == ']') {
                    break;
                } else {
                    pos++;
                }
            }
            if (pos < json.length()) pos++; // Skip ]
            newValues[key] = arr;
        } else if (json[pos] == 't' && json.Mid(pos, 4) == "true") {
            newValues[key] = true;
            pos += 4;
        } else if (json[pos] == 'f' && json.Mid(pos, 5) == "false") {
            newValues[key] = false;
            pos += 5;
        } else if (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')) {
            // Number
            size_t numStart = pos;
            bool hasDecimal = false;
            if (json[pos] == '-') pos++;
            while (pos < json.length() && 
                   ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '.')) {
                if (json[pos] == '.') hasDecimal = true;
                pos++;
            }
            wxString numStr = json.Mid(numStart, pos - numStart);
            if (hasDecimal) {
                double d;
                numStr.ToDouble(&d);
                newValues[key] = d;
            } else {
                long l;
                numStr.ToLong(&l);
                newValues[key] = static_cast<int>(l);
            }
        }
    }
    
    // Merge with defaults (keep defaults for keys not in file)
    for (const auto& pair : newValues) {
        m_values[pair.first] = pair.second;
    }
    
    return true;
}
