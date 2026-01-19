#ifndef EDITOR_WIDGET_H
#define EDITOR_WIDGET_H

#include "widget.h"
#include "editor.h"
#include "../theme/theme.h"

namespace BuiltinWidgets {

/**
 * Code editor widget.
 * The main editing area with syntax highlighting and file management.
 */
class EditorWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.editor";
        info.name = "Editor";
        info.description = "Code editor with syntax highlighting";
        info.location = WidgetLocation::Editor;
        info.priority = 100;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_editor = new Editor(parent);
        
        // Set up callbacks if frame is available
        auto* updateTitleFunc = context.Get<std::function<void()>>("updateTitle");
        if (updateTitleFunc) {
            m_editor->SetDirtyStateCallback([updateTitleFunc](bool) { (*updateTitleFunc)(); });
            m_editor->SetFileChangeCallback([updateTitleFunc](const wxString&) { (*updateTitleFunc)(); });
        }
        
        return m_editor;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        if (m_editor) {
            m_editor->ApplyTheme(ThemeManager::Instance().GetCurrentTheme());
        }
    }

    void OnFocus(wxWindow* window, WidgetContext& context) override {
        if (m_editor && m_editor->GetTextCtrl()) {
            m_editor->GetTextCtrl()->SetFocus();
        }
    }

    Editor* GetEditor() { return m_editor; }

    std::vector<wxString> GetCommands() const override {
        return {
            "file.new", "file.open", "file.save", "file.saveAs",
            "edit.undo", "edit.redo", "edit.cut", "edit.copy", "edit.paste",
            "edit.find", "edit.goToLine", "view.zoomIn", "view.zoomOut"
        };
    }

private:
    Editor* m_editor = nullptr;
};

} // namespace BuiltinWidgets

#endif // EDITOR_WIDGET_H
