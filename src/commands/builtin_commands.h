#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

/**
 * Orchestrator for registering all built-in commands.
 * 
 * This file delegates command registration to feature-specific modules,
 * promoting separation of concerns and easier extension.
 * 
 * To add new commands:
 * 1. Create a new *_commands.h file in the appropriate feature directory
 * 2. Implement a Register() function in that file's namespace
 * 3. Include the header and call the Register() function below
 */

// Feature-specific command registration headers
#include "app_commands.h"
#include "file_commands.h"
#include "edit_commands.h"
#include "view_commands.h"
#include "../ui/terminal_commands.h"
#include "../theme/theme_commands.h"
#include "../config/config_commands.h"
#include "../fs/remote_commands.h"

namespace BuiltinCommands {

/**
 * Register all built-in commands from all features.
 * Each feature module is responsible for registering its own commands.
 */
inline void RegisterAll() {
    // Application-level commands
    AppCommands::Register();
    
    // File operations
    FileCommands::Register();
    
    // Editor commands
    EditCommands::Register();
    ViewCommands::Register();
    
    // UI components
    TerminalCommands::Register();
    
    // Configuration and theming
    ThemeCommands::Register();
    ConfigCommands::Register();
    
    // Remote/SSH operations
    RemoteCommands::Register();
}

} // namespace BuiltinCommands

#endif // BUILTIN_COMMANDS_H
