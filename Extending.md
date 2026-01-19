# Extending ByteMuseHQ

This guide explains how to extend ByteMuseHQ with custom commands and visual widgets. The extension system is designed to be simple yet powerful, allowing you to add new functionality without modifying the core codebase.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Commands](#commands)
  - [Command Basics](#command-basics)
  - [Creating Commands](#creating-commands)
  - [Command Context](#command-context)
  - [Conditional Commands](#conditional-commands)
  - [Command Categories](#command-categories)
- [Widgets](#widgets)
  - [Widget Basics](#widget-basics)
  - [Creating Widgets](#creating-widgets)
  - [Widget Locations](#widget-locations)
  - [Widget Lifecycle](#widget-lifecycle)
  - [Theme Support](#theme-support)
- [Registration](#registration)
- [Examples](#examples)

---

## Architecture Overview

ByteMuseHQ uses two primary extension points:

1. **CommandRegistry** — Manages executable actions accessible via the command palette, menus, and keyboard shortcuts
2. **WidgetRegistry** — Manages visual UI components that can be placed in the sidebar, editor area, or bottom panel

Both systems use a registry pattern with singleton access, making it easy to register extensions from anywhere in the codebase.

```
┌─────────────────────────────────────────────────────────────┐
│                        MainFrame                            │
├─────────────┬───────────────────────────────┬───────────────┤
│   Sidebar   │          Editor Area          │               │
│   ┌───────┐ │  ┌─────────────────────────┐  │               │
│   │Widget │ │  │   EditorWidget          │  │               │
│   │ (tree)│ │  │                         │  │               │
│   │       │ │  │   - Syntax highlighting │  │               │
│   │       │ │  │   - File management     │  │               │
│   │       │ │  │                         │  │               │
│   └───────┘ │  └─────────────────────────┘  │               │
│             ├───────────────────────────────┤               │
│             │       Panel Area              │               │
│             │  ┌─────────────────────────┐  │               │
│             │  │   TerminalWidget        │  │               │
│             │  │   (or custom widgets)   │  │               │
│             │  └─────────────────────────┘  │               │
└─────────────┴───────────────────────────────┴───────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ Command Palette │
                    │ (Ctrl+Shift+P)  │
                    │                 │
                    │ > File: Open    │
                    │   Edit: Find    │
                    │   View: Zoom In │
                    └─────────────────┘
```

---

## Commands

### Command Basics

Commands are actions that can be executed from multiple places:
- **Command Palette** — Fuzzy-searchable list (`Ctrl+Shift+P`)
- **Menus** — File, Edit, View menus
- **Keyboard Shortcuts** — Configurable key bindings

Each command has:
- **ID** — Unique identifier (e.g., `"file.save"`, `"myext.doSomething"`)
- **Title** — Display name (e.g., `"Save"`, `"Do Something"`)
- **Category** — Grouping for the palette (e.g., `"File"`, `"Edit"`)
- **Description** — Optional help text
- **Shortcut** — Optional keyboard shortcut
- **Execute Handler** — Function called when the command runs
- **Enabled Handler** — Optional function to conditionally enable/disable

### Creating Commands

#### Method 1: Direct Construction

```cpp
#include "commands/command.h"
#include "commands/command_registry.h"

void RegisterMyCommands() {
    // Create a command
    auto cmd = std::make_shared<Command>(
        "myext.greet",      // ID
        "Say Hello",        // Title
        "My Extension"      // Category
    );
    
    // Configure the command
    cmd->SetDescription("Displays a friendly greeting");
    cmd->SetShortcut("Ctrl+Alt+H");
    
    // Set the execute handler
    cmd->SetExecuteHandler([](CommandContext& ctx) {
        wxMessageBox("Hello, World!", "Greeting", wxOK | wxICON_INFORMATION);
    });
    
    // Register with the global registry
    CommandRegistry::Instance().Register(cmd);
}
```

#### Method 2: Using the Helper Function

For convenience, use `MakeCommand` from `builtin_commands.h`:

```cpp
#include "commands/builtin_commands.h"

void RegisterMyCommands() {
    auto& registry = CommandRegistry::Instance();
    
    registry.Register(BuiltinCommands::MakeCommand(
        "myext.countLines",           // ID
        "Count Lines",                // Title
        "My Extension",               // Category
        "Ctrl+Alt+L",                 // Shortcut
        "Count lines in the editor",  // Description
        [](CommandContext& ctx) {     // Execute handler
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            if (editor) {
                int lines = editor->GetLineCount();
                wxMessageBox(wxString::Format("Lines: %d", lines), "Count");
            }
        },
        [](const CommandContext& ctx) {  // Enabled handler (optional)
            auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
            return editor != nullptr;
        }
    ));
}
```

### Command Context

The `CommandContext` provides access to application state during command execution:

```cpp
cmd->SetExecuteHandler([](CommandContext& ctx) {
    // Get the main window
    auto* window = ctx.Get<wxWindow>("window");
    
    // Get the main frame (for frame-specific operations)
    auto* frame = ctx.Get<MainFrame>("mainFrame");
    
    // Get the editor text control
    auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
    
    // Get the editor component (for file operations)
    auto* editorComponent = ctx.Get<Editor>("editorComponent");
    
    // Get the terminal component
    auto* terminal = ctx.Get<Terminal>("terminal");
    
    // Get the current file path (may be empty)
    auto* currentFile = ctx.Get<wxString>("currentFile");
});
```

**Available Context Keys:**

| Key | Type | Description |
|-----|------|-------------|
| `"window"` | `wxWindow*` | Main frame window |
| `"mainFrame"` | `MainFrame*` | Main frame instance |
| `"editor"` | `wxStyledTextCtrl*` | Editor text control |
| `"editorComponent"` | `Editor*` | Editor wrapper class |
| `"terminal"` | `Terminal*` | Terminal component |
| `"currentFile"` | `wxString*` | Path to currently open file |

### Conditional Commands

Commands can be conditionally enabled based on application state:

```cpp
// Only enable when there's a selection
cmd->SetEnabledHandler([](const CommandContext& ctx) {
    auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
    if (!editor) return false;
    
    return editor->GetSelectionStart() != editor->GetSelectionEnd();
});

// Only enable when the file has been modified
cmd->SetEnabledHandler([](const CommandContext& ctx) {
    auto* editor = ctx.Get<Editor>("editorComponent");
    return editor && editor->IsModified();
});

// Only enable when terminal is visible
cmd->SetEnabledHandler([](const CommandContext& ctx) {
    auto* frame = ctx.Get<MainFrame>("mainFrame");
    return frame && frame->IsTerminalVisible();
});
```

Disabled commands are hidden from the command palette and cannot be executed.

### Command Categories

Organize commands into categories for better discoverability:

| Category | Purpose |
|----------|---------|
| `"File"` | File operations (new, open, save) |
| `"Edit"` | Text editing (undo, copy, paste) |
| `"View"` | Display settings (zoom, word wrap) |
| `"Terminal"` | Terminal operations |
| `"Preferences"` | Settings and themes |
| `"Help"` | Help and about |
| `"Application"` | App-level commands (quit) |

Custom categories are supported — just use any string:

```cpp
auto cmd = std::make_shared<Command>("git.commit", "Commit", "Git");
```

---

## Widgets

### Widget Basics

Widgets are visual UI components that can be placed in different areas of the main window. The widget system provides:

- **Lazy instantiation** — Widgets are created only when needed
- **Lifecycle management** — Proper initialization and cleanup
- **Theme support** — Automatic theme change notifications
- **Registry pattern** — Easy registration and lookup

### Creating Widgets

Create a widget by inheriting from the `Widget` base class:

```cpp
#include "ui/widget.h"

class MyPanelWidget : public Widget {
public:
    /**
     * Return metadata about this widget.
     */
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "myext.panel";              // Unique ID
        info.name = "My Panel";               // Display name
        info.description = "A custom panel";  // Brief description
        info.location = WidgetLocation::Panel; // Where to place it
        info.priority = 50;                   // Order (higher = first)
        info.showByDefault = true;            // Show on startup
        return info;
    }

    /**
     * Create the widget's window.
     * Called once when the widget is first shown.
     */
    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        // Add a label
        m_label = new wxStaticText(m_panel, wxID_ANY, "Hello from My Panel!");
        sizer->Add(m_label, 0, wxALL, 10);
        
        // Add a button
        auto* button = new wxButton(m_panel, wxID_ANY, "Click Me");
        button->Bind(wxEVT_BUTTON, &MyPanelWidget::OnButtonClick, this);
        sizer->Add(button, 0, wxALL, 10);
        
        m_panel->SetSizer(sizer);
        return m_panel;
    }

    /**
     * Handle theme changes.
     */
    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (theme && m_panel) {
            m_panel->SetBackgroundColour(theme->ui.panelBackground);
            m_label->SetForegroundColour(theme->ui.panelForeground);
            m_panel->Refresh();
        }
    }

    /**
     * Handle focus events.
     */
    void OnFocus(wxWindow* window, WidgetContext& context) override {
        // Focus the first control
        if (m_panel) {
            m_panel->SetFocus();
        }
    }

    /**
     * Return commands provided by this widget.
     */
    std::vector<wxString> GetCommands() const override {
        return { "myext.panel.refresh", "myext.panel.clear" };
    }

private:
    wxPanel* m_panel = nullptr;
    wxStaticText* m_label = nullptr;

    void OnButtonClick(wxCommandEvent& event) {
        wxMessageBox("Button clicked!", "My Panel");
    }
};
```

### Widget Locations

Widgets can be placed in different areas:

```cpp
enum class WidgetLocation {
    Sidebar,    // Left panel (like file tree)
    Editor,     // Main editor area (center)
    Panel,      // Bottom panel (like terminal)
    StatusBar,  // Status bar area
    ToolBar     // Tool bar area
};
```

**Location Guidelines:**

| Location | Use Case | Example |
|----------|----------|---------|
| `Sidebar` | Navigation, file browsers, outlines | File Explorer, Git Changes |
| `Editor` | Main content editing | Code Editor, Diff Viewer |
| `Panel` | Output, terminals, logs | Terminal, Build Output |
| `StatusBar` | Quick status information | Line/Column, Encoding |
| `ToolBar` | Frequently used actions | Build, Run, Debug |

### Widget Lifecycle

Widgets have several lifecycle methods you can override:

```cpp
class MyWidget : public Widget {
public:
    // Required: Return widget metadata
    WidgetInfo GetInfo() const override;
    
    // Required: Create the widget's window
    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override;
    
    // Optional: Handle theme changes
    void OnThemeChanged(wxWindow* window, WidgetContext& context) override;
    
    // Optional: Handle focus
    void OnFocus(wxWindow* window, WidgetContext& context) override;
    
    // Optional: Called when widget is about to be shown
    void OnShow(wxWindow* window, WidgetContext& context) override;
    
    // Optional: Called when widget is about to be hidden
    void OnHide(wxWindow* window, WidgetContext& context) override;
    
    // Optional: Return associated command IDs
    std::vector<wxString> GetCommands() const override;
};
```

### Theme Support

Widgets should respond to theme changes for a consistent look:

```cpp
void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
    auto theme = ThemeManager::Instance().GetCurrentTheme();
    if (!theme) return;
    
    // Apply UI colors
    window->SetBackgroundColour(theme->ui.panelBackground);
    window->SetForegroundColour(theme->ui.panelForeground);
    
    // Style any child controls
    for (auto* child : window->GetChildren()) {
        if (auto* textCtrl = dynamic_cast<wxTextCtrl*>(child)) {
            textCtrl->SetBackgroundColour(theme->ui.inputBackground);
            textCtrl->SetForegroundColour(theme->ui.inputForeground);
        }
    }
    
    window->Refresh();
}
```

**Available Theme Colors:**

```cpp
struct ThemeUI {
    wxColour windowBackground;
    wxColour windowForeground;
    wxColour sidebarBackground;
    wxColour sidebarForeground;
    wxColour panelBackground;
    wxColour panelForeground;
    wxColour inputBackground;
    wxColour inputForeground;
    wxColour border;
    wxColour selection;
    wxColour selectionForeground;
    wxColour accent;
};
```

---

## Registration

### Registering Commands

Register commands in a central location, typically called from `MainFrame::RegisterCommands()`:

```cpp
// In your extension file
namespace MyExtension {

void RegisterCommands() {
    auto& registry = CommandRegistry::Instance();
    
    registry.Register(std::make_shared<Command>("myext.cmd1", "Command 1", "My Ext"));
    registry.Register(std::make_shared<Command>("myext.cmd2", "Command 2", "My Ext"));
}

} // namespace MyExtension
```

Then call it during initialization:

```cpp
// In frame.cpp
void MainFrame::RegisterCommands() {
    BuiltinCommands::RegisterAll();
    MyExtension::RegisterCommands();  // Add your extension
}
```

### Registering Widgets

Register widgets with the `WidgetRegistry`:

```cpp
namespace MyExtension {

void RegisterWidgets() {
    auto& registry = WidgetRegistry::Instance();
    
    // Method 1: Register a factory (lazy instantiation)
    registry.Register("myext.panel", []() -> WidgetPtr {
        return std::make_shared<MyPanelWidget>();
    });
    
    // Method 2: Register an instance directly
    registry.Register(std::make_shared<MyOtherWidget>());
}

} // namespace MyExtension
```

Call during initialization:

```cpp
// In frame.cpp
void MainFrame::RegisterWidgets() {
    BuiltinWidgets::RegisterAll();
    MyExtension::RegisterWidgets();  // Add your extension
}
```

---

## Examples

### Example 1: Word Count Command

```cpp
#include "commands/command.h"
#include "commands/command_registry.h"
#include <wx/stc/stc.h>

void RegisterWordCountCommand() {
    auto cmd = std::make_shared<Command>("tools.wordCount", "Word Count", "Tools");
    cmd->SetDescription("Count words in the current document");
    cmd->SetShortcut("Ctrl+Shift+W");
    
    cmd->SetExecuteHandler([](CommandContext& ctx) {
        auto* editor = ctx.Get<wxStyledTextCtrl>("editor");
        if (!editor) return;
        
        wxString text = editor->GetText();
        
        // Count words (simple whitespace split)
        int words = 0;
        bool inWord = false;
        for (auto ch : text) {
            if (wxIsspace(ch)) {
                inWord = false;
            } else if (!inWord) {
                inWord = true;
                words++;
            }
        }
        
        int chars = text.Length();
        int lines = editor->GetLineCount();
        
        wxMessageBox(
            wxString::Format("Words: %d\nCharacters: %d\nLines: %d", words, chars, lines),
            "Word Count",
            wxOK | wxICON_INFORMATION
        );
    });
    
    CommandRegistry::Instance().Register(cmd);
}
```

### Example 2: Output Panel Widget

```cpp
#include "ui/widget.h"
#include "../theme/theme.h"

class OutputWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "tools.output";
        info.name = "Output";
        info.description = "View build and task output";
        info.location = WidgetLocation::Panel;
        info.priority = 80;
        info.showByDefault = false;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        // Header with clear button
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
        headerSizer->Add(new wxStaticText(m_panel, wxID_ANY, "Output"), 
                         0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
        headerSizer->AddStretchSpacer();
        
        auto* clearBtn = new wxButton(m_panel, wxID_ANY, "Clear", 
                                       wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        clearBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Clear(); });
        headerSizer->Add(clearBtn, 0, wxRIGHT, 5);
        
        sizer->Add(headerSizer, 0, wxEXPAND | wxALL, 2);
        
        // Output text area
        m_output = new wxTextCtrl(m_panel, wxID_ANY, "", 
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
        m_output->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, 
                                  wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        sizer->Add(m_output, 1, wxEXPAND | wxALL, 2);
        
        m_panel->SetSizer(sizer);
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme) return;
        
        m_panel->SetBackgroundColour(theme->ui.panelBackground);
        m_output->SetBackgroundColour(theme->ui.inputBackground);
        m_output->SetForegroundColour(theme->ui.inputForeground);
        m_panel->Refresh();
    }

    // Public API for appending output
    void AppendText(const wxString& text) {
        if (m_output) {
            m_output->AppendText(text);
        }
    }

    void Clear() {
        if (m_output) {
            m_output->Clear();
        }
    }

    std::vector<wxString> GetCommands() const override {
        return { "output.clear", "output.show" };
    }

private:
    wxPanel* m_panel = nullptr;
    wxTextCtrl* m_output = nullptr;
};

// Registration
void RegisterOutputWidget() {
    WidgetRegistry::Instance().Register("tools.output", []() -> WidgetPtr {
        return std::make_shared<OutputWidget>();
    });
    
    // Also register associated commands
    CommandRegistry::Instance().Register(
        std::make_shared<Command>("output.clear", "Clear Output", "Output")
    );
}
```

### Example 3: Git Status Sidebar Widget

```cpp
#include "ui/widget.h"
#include <wx/listctrl.h>

class GitStatusWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "git.status";
        info.name = "Git Changes";
        info.description = "View uncommitted changes";
        info.location = WidgetLocation::Sidebar;
        info.priority = 50;  // Below file tree (priority 100)
        info.showByDefault = false;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        // Header
        auto* header = new wxStaticText(m_panel, wxID_ANY, "CHANGES");
        sizer->Add(header, 0, wxALL, 5);
        
        // File list
        m_list = new wxListCtrl(m_panel, wxID_ANY, 
                                 wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
        m_list->InsertColumn(0, "Status", wxLIST_FORMAT_LEFT, 30);
        m_list->InsertColumn(1, "File", wxLIST_FORMAT_LEFT, 200);
        
        sizer->Add(m_list, 1, wxEXPAND | wxALL, 2);
        
        // Refresh button
        auto* refreshBtn = new wxButton(m_panel, wxID_ANY, "Refresh");
        refreshBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshStatus(); });
        sizer->Add(refreshBtn, 0, wxEXPAND | wxALL, 5);
        
        m_panel->SetSizer(sizer);
        
        // Initial refresh
        RefreshStatus();
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme) return;
        
        m_panel->SetBackgroundColour(theme->ui.sidebarBackground);
        m_list->SetBackgroundColour(theme->ui.sidebarBackground);
        m_list->SetForegroundColour(theme->ui.sidebarForeground);
        m_panel->Refresh();
    }

    std::vector<wxString> GetCommands() const override {
        return { "git.refresh", "git.stageAll", "git.commit" };
    }

private:
    wxPanel* m_panel = nullptr;
    wxListCtrl* m_list = nullptr;

    void RefreshStatus() {
        if (!m_list) return;
        m_list->DeleteAllItems();
        
        // Run git status (simplified example)
        wxArrayString output;
        wxExecute("git status --porcelain", output);
        
        for (const auto& line : output) {
            if (line.Length() >= 3) {
                wxString status = line.Left(2).Trim();
                wxString file = line.Mid(3);
                
                long idx = m_list->InsertItem(m_list->GetItemCount(), status);
                m_list->SetItem(idx, 1, file);
            }
        }
    }
};
```

---

## Best Practices

### Command Design

1. **Use descriptive IDs** — Follow the pattern `category.action` (e.g., `file.save`, `edit.undo`)
2. **Provide shortcuts** — Common actions should have keyboard shortcuts
3. **Add descriptions** — Help users understand what the command does
4. **Handle errors gracefully** — Check for null pointers, show user-friendly error messages
5. **Use enabled handlers** — Hide irrelevant commands from the palette

### Widget Design

1. **Keep widgets focused** — Each widget should have a single responsibility
2. **Support themes** — Always implement `OnThemeChanged()`
3. **Use appropriate locations** — Choose the right `WidgetLocation` for your widget
4. **Clean up resources** — Properly manage memory in destructors
5. **Provide commands** — Register commands for widget-specific actions

### Performance

1. **Lazy initialization** — Use widget factories for deferred creation
2. **Avoid blocking** — Use background threads for long operations
3. **Minimize redraws** — Batch UI updates when possible

---

## API Reference

### Command Class

```cpp
class Command {
    // Constructor
    Command(const wxString& id, const wxString& title, const wxString& category = "");
    
    // Getters
    const wxString& GetId() const;
    const wxString& GetTitle() const;
    const wxString& GetCategory() const;
    const wxString& GetDescription() const;
    const wxString& GetShortcut() const;
    wxString GetDisplayString() const;  // "Category: Title"
    
    // Fluent setters
    Command& SetDescription(const wxString& desc);
    Command& SetShortcut(const wxString& shortcut);
    Command& SetExecuteHandler(ExecuteFunc func);
    Command& SetEnabledHandler(EnabledFunc func);
    
    // Execution
    bool IsEnabled(const CommandContext& context) const;
    void Execute(CommandContext& context);
};
```

### CommandRegistry Class

```cpp
class CommandRegistry {
    static CommandRegistry& Instance();
    
    void Register(CommandPtr command);
    void RegisterAll(const std::vector<CommandPtr>& commands);
    void Unregister(const wxString& id);
    
    CommandPtr GetCommand(const wxString& id) const;
    std::vector<CommandPtr> GetAllCommands() const;
    std::vector<CommandPtr> GetCommandsByCategory(const wxString& category) const;
    std::vector<wxString> GetCategories() const;
    std::vector<CommandPtr> Search(const wxString& query, const CommandContext& context) const;
};
```

### Widget Class

```cpp
class Widget {
    virtual WidgetInfo GetInfo() const = 0;
    virtual wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) = 0;
    
    virtual void OnThemeChanged(wxWindow* window, WidgetContext& context);
    virtual void OnFocus(wxWindow* window, WidgetContext& context);
    virtual void OnShow(wxWindow* window, WidgetContext& context);
    virtual void OnHide(wxWindow* window, WidgetContext& context);
    virtual std::vector<wxString> GetCommands() const;
};
```

### WidgetRegistry Class

```cpp
class WidgetRegistry {
    static WidgetRegistry& Instance();
    
    void Register(const wxString& id, WidgetFactory factory);
    void Register(WidgetPtr widget);
    void Unregister(const wxString& id);
    
    WidgetPtr GetWidget(const wxString& id);
    std::vector<wxString> GetAllIds() const;
    std::vector<WidgetPtr> GetWidgetsByLocation(WidgetLocation location);
    std::vector<WidgetPtr> GetAllWidgets() const;
};
```

---

## Troubleshooting

**Command not appearing in palette:**
- Ensure the command is registered before the palette opens
- Check if the enabled handler returns `false`
- Verify the command ID is unique

**Widget not showing:**
- Verify the widget is registered before the frame is created
- Check that `showByDefault` is set correctly
- Ensure `CreateWindow()` returns a valid window

**Theme not applying:**
- Implement `OnThemeChanged()` in your widget
- Call `window->Refresh()` after applying colors
- Check that the theme manager has listeners set up
