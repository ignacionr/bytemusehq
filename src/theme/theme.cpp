#include "theme.h"
#include "../config/config.h"

ThemeManager::ThemeManager() {
    CreateBuiltinThemes();
}

void ThemeManager::Initialize() {
    // Load current theme from config
    auto& config = Config::Instance();
    wxString themeId = config.GetString("theme.current", "dark");
    
    if (!SetCurrentTheme(themeId)) {
        // Fallback to dark theme
        SetCurrentTheme("dark");
    }
    
    // Listen for config changes
    config.AddListener("theme.current", [this](const wxString& key, const ConfigValue& value) {
        if (auto* str = std::get_if<wxString>(&value)) {
            SetCurrentTheme(*str);
        }
    });
}

void ThemeManager::CreateBuiltinThemes() {
    RegisterTheme(CreateDarkTheme());
    RegisterTheme(CreateLightTheme());
}

ThemePtr ThemeManager::CreateDarkTheme() {
    auto theme = std::make_shared<Theme>();
    
    theme->id = "dark";
    theme->name = "Dark";
    theme->description = "Default dark theme with comfortable contrast";
    theme->isDark = true;
    
    // Editor colors - Modern dark theme inspired by popular editors
    theme->editor.background = wxColour(30, 30, 30);           // #1e1e1e
    theme->editor.foreground = wxColour(212, 212, 212);        // #d4d4d4
    theme->editor.lineNumberBackground = wxColour(30, 30, 30);
    theme->editor.lineNumberForeground = wxColour(133, 133, 133); // #858585
    theme->editor.caretLine = wxColour(40, 40, 40);            // Subtle highlight
    theme->editor.caret = wxColour(255, 255, 255);
    theme->editor.selection = wxColour(38, 79, 120);           // #264f78
    theme->editor.selectionForeground = wxColour(255, 255, 255);
    theme->editor.whitespace = wxColour(64, 64, 64);
    theme->editor.indentGuide = wxColour(64, 64, 64);
    
    // Syntax highlighting - VSCode-inspired
    theme->editor.comment = wxColour(106, 153, 85);            // #6a9955 - Green
    theme->editor.keyword = wxColour(86, 156, 214);            // #569cd6 - Blue
    theme->editor.string = wxColour(206, 145, 120);            // #ce9178 - Orange/brown
    theme->editor.number = wxColour(181, 206, 168);            // #b5cea8 - Light green
    theme->editor.operator_ = wxColour(212, 212, 212);         // Same as text
    theme->editor.preprocessor = wxColour(155, 155, 255);      // #9b9bff - Purple-ish
    theme->editor.identifier = wxColour(156, 220, 254);        // #9cdcfe - Light blue
    theme->editor.type = wxColour(78, 201, 176);               // #4ec9b0 - Teal
    theme->editor.function = wxColour(220, 220, 170);          // #dcdcaa - Yellow
    
    // Terminal colors
    theme->terminal.background = wxColour(24, 24, 24);         // Slightly darker
    theme->terminal.foreground = wxColour(204, 204, 204);
    theme->terminal.inputBackground = wxColour(36, 36, 36);
    theme->terminal.inputForeground = wxColour(255, 255, 255);
    theme->terminal.prompt = wxColour(86, 156, 214);           // Blue prompt
    theme->terminal.error = wxColour(244, 135, 113);           // Red for errors
    
    // UI colors
    theme->ui.windowBackground = wxColour(30, 30, 30);
    theme->ui.panelBackground = wxColour(37, 37, 38);          // #252526
    theme->ui.sidebarBackground = wxColour(37, 37, 38);
    theme->ui.sidebarForeground = wxColour(204, 204, 204);
    theme->ui.sidebarSelection = wxColour(55, 55, 55);
    theme->ui.statusBarBackground = wxColour(0, 122, 204);     // Blue status bar
    theme->ui.statusBarForeground = wxColour(255, 255, 255);
    theme->ui.titleBarBackground = wxColour(60, 60, 60);
    theme->ui.titleBarForeground = wxColour(204, 204, 204);
    theme->ui.border = wxColour(60, 60, 60);
    theme->ui.separator = wxColour(60, 60, 60);
    theme->ui.scrollbar = wxColour(79, 79, 79);
    theme->ui.scrollbarHover = wxColour(100, 100, 100);
    
    // Palette colors
    theme->palette.background = wxColour(37, 37, 38);
    theme->palette.foreground = wxColour(204, 204, 204);
    theme->palette.inputBackground = wxColour(60, 60, 60);
    theme->palette.inputForeground = wxColour(255, 255, 255);
    theme->palette.itemHover = wxColour(47, 47, 48);
    theme->palette.itemSelected = wxColour(4, 57, 94);         // Accent blue
    theme->palette.border = wxColour(69, 69, 69);
    theme->palette.shadow = wxColour(0, 0, 0);
    theme->palette.categoryForeground = wxColour(128, 128, 128);
    theme->palette.shortcutForeground = wxColour(128, 128, 128);
    
    return theme;
}

ThemePtr ThemeManager::CreateLightTheme() {
    auto theme = std::make_shared<Theme>();
    
    theme->id = "light";
    theme->name = "Light";
    theme->description = "Clean light theme for bright environments";
    theme->isDark = false;
    
    // Editor colors
    theme->editor.background = wxColour(255, 255, 255);
    theme->editor.foreground = wxColour(0, 0, 0);
    theme->editor.lineNumberBackground = wxColour(255, 255, 255);
    theme->editor.lineNumberForeground = wxColour(150, 150, 150);
    theme->editor.caretLine = wxColour(245, 245, 245);
    theme->editor.caret = wxColour(0, 0, 0);
    theme->editor.selection = wxColour(173, 214, 255);
    theme->editor.selectionForeground = wxColour(0, 0, 0);
    theme->editor.whitespace = wxColour(200, 200, 200);
    theme->editor.indentGuide = wxColour(220, 220, 220);
    
    // Syntax highlighting
    theme->editor.comment = wxColour(0, 128, 0);               // Green
    theme->editor.keyword = wxColour(0, 0, 255);               // Blue
    theme->editor.string = wxColour(163, 21, 21);              // Dark red
    theme->editor.number = wxColour(9, 134, 88);               // Green
    theme->editor.operator_ = wxColour(0, 0, 0);
    theme->editor.preprocessor = wxColour(128, 0, 128);        // Purple
    theme->editor.identifier = wxColour(0, 16, 128);           // Dark blue
    theme->editor.type = wxColour(38, 127, 153);               // Teal
    theme->editor.function = wxColour(121, 94, 38);            // Brown
    
    // Terminal colors
    theme->terminal.background = wxColour(255, 255, 255);
    theme->terminal.foreground = wxColour(0, 0, 0);
    theme->terminal.inputBackground = wxColour(245, 245, 245);
    theme->terminal.inputForeground = wxColour(0, 0, 0);
    theme->terminal.prompt = wxColour(0, 100, 200);
    theme->terminal.error = wxColour(200, 0, 0);
    
    // UI colors
    theme->ui.windowBackground = wxColour(243, 243, 243);
    theme->ui.panelBackground = wxColour(243, 243, 243);
    theme->ui.sidebarBackground = wxColour(243, 243, 243);
    theme->ui.sidebarForeground = wxColour(51, 51, 51);
    theme->ui.sidebarSelection = wxColour(200, 200, 200);
    theme->ui.statusBarBackground = wxColour(0, 122, 204);
    theme->ui.statusBarForeground = wxColour(255, 255, 255);
    theme->ui.titleBarBackground = wxColour(221, 221, 221);
    theme->ui.titleBarForeground = wxColour(51, 51, 51);
    theme->ui.border = wxColour(200, 200, 200);
    theme->ui.separator = wxColour(200, 200, 200);
    theme->ui.scrollbar = wxColour(180, 180, 180);
    theme->ui.scrollbarHover = wxColour(150, 150, 150);
    
    // Palette colors
    theme->palette.background = wxColour(255, 255, 255);
    theme->palette.foreground = wxColour(51, 51, 51);
    theme->palette.inputBackground = wxColour(255, 255, 255);
    theme->palette.inputForeground = wxColour(0, 0, 0);
    theme->palette.itemHover = wxColour(232, 232, 232);
    theme->palette.itemSelected = wxColour(0, 122, 204);
    theme->palette.border = wxColour(200, 200, 200);
    theme->palette.shadow = wxColour(100, 100, 100);
    theme->palette.categoryForeground = wxColour(128, 128, 128);
    theme->palette.shortcutForeground = wxColour(128, 128, 128);
    
    return theme;
}

ThemePtr ThemeManager::GetTheme(const wxString& id) const {
    auto it = m_themes.find(id);
    if (it != m_themes.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ThemePtr> ThemeManager::GetAllThemes() const {
    std::vector<ThemePtr> result;
    result.reserve(m_themes.size());
    for (const auto& pair : m_themes) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<wxString> ThemeManager::GetThemeIds() const {
    std::vector<wxString> result;
    result.reserve(m_themes.size());
    for (const auto& pair : m_themes) {
        result.push_back(pair.first);
    }
    return result;
}

bool ThemeManager::SetCurrentTheme(const wxString& id) {
    auto theme = GetTheme(id);
    if (!theme) {
        return false;
    }
    
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        
        // Save to config
        auto& config = Config::Instance();
        config.Set("theme.current", id);
        config.Save();
        
        NotifyListeners();
    }
    
    return true;
}

void ThemeManager::RegisterTheme(ThemePtr theme) {
    if (theme) {
        m_themes[theme->id] = theme;
    }
}

void ThemeManager::UnregisterTheme(const wxString& id) {
    // Don't allow removing builtin themes
    if (id != "dark" && id != "light") {
        m_themes.erase(id);
    }
}

int ThemeManager::AddChangeListener(ThemeChangeCallback callback) {
    int id = m_nextListenerId++;
    m_listeners.push_back({id, std::move(callback)});
    return id;
}

void ThemeManager::RemoveChangeListener(int id) {
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
            [id](const Listener& l) { return l.id == id; }),
        m_listeners.end());
}

void ThemeManager::NotifyListeners() {
    for (const auto& listener : m_listeners) {
        if (listener.callback) {
            listener.callback(m_currentTheme);
        }
    }
}
