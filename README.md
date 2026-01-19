# ByteMuseHQ

A lightweight, cross-platform code editor built with wxWidgets and C++17. ByteMuseHQ features a VS Code-style command palette, integrated terminal, customizable themes, and an extensible plugin architecture.

![ByteMuseHQ](https://img.shields.io/badge/version-1.0.0-blue.svg)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange.svg)
![wxWidgets](https://img.shields.io/badge/wxWidgets-3.2-green.svg)

## Features

- **Code Editor** — Syntax highlighting for C++, Python, JavaScript, and more
- **Command Palette** — VS Code-style fuzzy search (`Ctrl+Shift+P`)
- **File Explorer** — Tree-based file browser sidebar
- **Integrated Terminal** — Built-in shell access (`Ctrl+\``)
- **Theming** — Dark and light themes with full customization
- **Extensible Architecture** — Plugin system for commands and widgets

## Building

### Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.10 or later
- wxWidgets 3.2+ (with `stc` component)

### Using Nix (Recommended)

The project includes a Nix flake for reproducible builds:

```bash
# Enter development shell
nix develop

# Configure and build
cmake -B build -S .
cmake --build build

# Run
./build/bytemusehq
```

### Manual Build

```bash
# Install wxWidgets (Ubuntu/Debian)
sudo apt install libwxgtk3.2-dev libwxgtk-stc-dev

# Install wxWidgets (macOS)
brew install wxwidgets

# Configure and build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./bytemusehq
```

### Windows Build

```powershell
# Install vcpkg and wxWidgets
vcpkg install wxwidgets:x64-windows

# Configure with vcpkg toolchain
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release
```

## Usage

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+P` | Open Command Palette |
| `Ctrl+N` | New File |
| `Ctrl+O` | Open File |
| `Ctrl+S` | Save File |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Find |
| `Ctrl+G` | Go to Line |
| `Ctrl+\`` | Toggle Terminal |
| `Ctrl++` | Zoom In |
| `Ctrl+-` | Zoom Out |
| `Ctrl+0` | Reset Zoom |
| `Ctrl+Q` | Quit |

### Command Palette

Press `Ctrl+Shift+P` to open the command palette. Start typing to filter commands:

- **File: New File** — Create a new empty file
- **File: Open File** — Open an existing file
- **Edit: Find** — Search in the current file
- **View: Toggle Word Wrap** — Enable/disable word wrapping
- **Terminal: Toggle Terminal** — Show/hide the integrated terminal
- **Preferences: Select Color Theme** — Switch between themes

## Project Structure

```
bytemusehq/
├── src/
│   ├── main.cpp              # Application entry point
│   ├── commands/
│   │   ├── command.h         # Command base class
│   │   ├── command_registry.h # Command registration
│   │   ├── command_palette.h/cpp # Palette UI
│   │   └── builtin_commands.h # Built-in commands
│   ├── ui/
│   │   ├── frame.h/cpp       # Main window
│   │   ├── editor.h/cpp      # Code editor component
│   │   ├── terminal.h/cpp    # Terminal component
│   │   ├── widget.h          # Widget plugin base class
│   │   └── builtin_widgets.h # Built-in widget implementations
│   ├── config/
│   │   └── config.h/cpp      # Configuration management
│   └── theme/
│       └── theme.h/cpp       # Theme system
├── CMakeLists.txt
├── flake.nix                 # Nix flake for builds
└── README.md
```

## Extending ByteMuseHQ

ByteMuseHQ supports two types of extensions:

1. **Commands** — Actions that can be executed from the command palette
2. **Widgets** — Visual components (sidebar panels, editor tabs, bottom panels)

See [Extending.md](Extending.md) for detailed documentation on creating extensions.

### Quick Example: Adding a Command

```cpp
#include "commands/command.h"
#include "commands/command_registry.h"

void RegisterMyCommands() {
    auto cmd = std::make_shared<Command>("myext.hello", "Hello World", "My Extension");
    cmd->SetDescription("Shows a hello message");
    cmd->SetExecuteHandler([](CommandContext& ctx) {
        wxMessageBox("Hello from my extension!", "Hello");
    });
    
    CommandRegistry::Instance().Register(cmd);
}
```

### Quick Example: Adding a Widget

```cpp
#include "ui/widget.h"

class MyPanelWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "myext.panel";
        info.name = "My Panel";
        info.location = WidgetLocation::Panel;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& ctx) override {
        auto* panel = new wxPanel(parent);
        // Build your UI...
        return panel;
    }
};
```

## Configuration

Configuration is stored in `~/.config/bytemusehq/config.json` (Linux/macOS) or `%APPDATA%\bytemusehq\config.json` (Windows).

```json
{
    "theme": "dark",
    "editor": {
        "fontSize": 12,
        "tabSize": 4,
        "wordWrap": false,
        "showLineNumbers": true
    },
    "terminal": {
        "shell": "/bin/zsh"
    }
}
```

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [wxWidgets](https://www.wxwidgets.org/) — Cross-platform GUI toolkit
- [Scintilla](https://www.scintilla.org/) — Code editing component (via wxStyledTextCtrl)
