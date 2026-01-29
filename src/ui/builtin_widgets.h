#ifndef BUILTIN_WIDGETS_H
#define BUILTIN_WIDGETS_H

/**
 * Built-in widget implementations for ByteMuseHQ.
 * 
 * These widgets provide the core UI components:
 * - FileTreeWidget: File browser sidebar
 * - EditorWidget: Code editor (center)
 * - TerminalWidget: Integrated terminal (bottom panel)
 * - TimerWidget: Pomodoro focus timer (sidebar)
 * - JiraWidget: JIRA issue tracker integration (sidebar)
 * - GeminiChatWidget: AI chat with Google Gemini (sidebar)
 * 
 * Each widget is defined in its own header file for modularity.
 */

// Include individual widget headers
#include "file_tree_widget.h"
#include "editor_widget.h"
#include "terminal_widget.h"
#include "timer_widget.h"
#include "jira_widget.h"
#include "gemini_chat_widget.h"

namespace BuiltinWidgets {

// ============================================================================
// Registration
// ============================================================================

/**
 * Register all built-in widgets with the WidgetRegistry.
 * Called during application initialization.
 */
inline void RegisterAll() {
    auto& registry = WidgetRegistry::Instance();
    
    // Register factories for lazy instantiation
    registry.Register("core.fileTree", []() -> WidgetPtr {
        return std::make_shared<FileTreeWidget>();
    });
    
    registry.Register("core.editor", []() -> WidgetPtr {
        return std::make_shared<EditorWidget>();
    });
    
    registry.Register("core.terminal", []() -> WidgetPtr {
        return std::make_shared<TerminalWidget>();
    });
    
    registry.Register("core.timer", []() -> WidgetPtr {
        return std::make_shared<TimerWidget>();
    });
    
    registry.Register("core.jira", []() -> WidgetPtr {
        return std::make_shared<JiraWidget>();
    });
    
    registry.Register("core.geminiChat", []() -> WidgetPtr {
        return std::make_shared<GeminiChatWidget>();
    });
}

} // namespace BuiltinWidgets

#endif // BUILTIN_WIDGETS_H

