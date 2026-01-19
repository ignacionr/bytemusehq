#ifndef BUILTIN_WIDGETS_H
#define BUILTIN_WIDGETS_H

#include "widget.h"
#include "editor.h"
#include "terminal.h"
#include "../theme/theme.h"
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>

/**
 * Built-in widget implementations for ByteMuseHQ.
 * 
 * These widgets provide the core UI components:
 * - FileTreeWidget: File browser sidebar
 * - EditorWidget: Code editor (center)
 * - TerminalWidget: Integrated terminal (bottom panel)
 */
namespace BuiltinWidgets {

// ============================================================================
// FileTreeWidget - Sidebar file browser
// ============================================================================

/**
 * Tree item data to store file paths.
 */
class PathData : public wxTreeItemData {
public:
    PathData(const wxString& path) : m_path(path) {}
    const wxString& GetPath() const { return m_path; }
private:
    wxString m_path;
};

/**
 * File tree sidebar widget.
 * Displays the workspace directory structure for file navigation.
 */
class FileTreeWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.fileTree";
        info.name = "File Explorer";
        info.description = "Browse and open files in the workspace";
        info.location = WidgetLocation::Sidebar;
        info.priority = 100;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        m_treeCtrl = new wxTreeCtrl(m_panel, wxID_ANY);
        sizer->Add(m_treeCtrl, 1, wxEXPAND);
        m_panel->SetSizer(sizer);
        
        // Store context for callbacks
        m_context = &context;
        
        // Populate with current directory
        wxString currentDir = wxGetCwd();
        wxTreeItemId rootId = m_treeCtrl->AddRoot(currentDir);
        PopulateTree(currentDir, rootId);
        m_treeCtrl->Expand(rootId);
        
        // Bind events
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &FileTreeWidget::OnItemActivated, this);
        m_treeCtrl->Bind(wxEVT_TREE_ITEM_EXPANDING, &FileTreeWidget::OnItemExpanding, this);
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme || !m_panel || !m_treeCtrl) return;
        
        m_panel->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetBackgroundColour(theme->ui.sidebarBackground);
        m_treeCtrl->SetForegroundColour(theme->ui.sidebarForeground);
        m_panel->Refresh();
    }

    wxTreeCtrl* GetTreeCtrl() { return m_treeCtrl; }

    std::vector<wxString> GetCommands() const override {
        return {
            "fileTree.refresh",
            "fileTree.collapseAll"
        };
    }

private:
    wxPanel* m_panel = nullptr;
    wxTreeCtrl* m_treeCtrl = nullptr;
    WidgetContext* m_context = nullptr;

    void PopulateTree(const wxString& path, wxTreeItemId parentItem) {
        wxDir dir(path);
        if (!dir.IsOpened()) return;
        
        wxString filename;
        bool cont = dir.GetFirst(&filename);
        
        while (cont) {
            if (!filename.StartsWith(".")) {
                wxString fullPath = wxFileName(path, filename).GetFullPath();
                
                if (wxDir::Exists(fullPath)) {
                    wxTreeItemId itemId = m_treeCtrl->AppendItem(
                        parentItem, filename, -1, -1, new PathData(fullPath));
                    m_treeCtrl->AppendItem(itemId, ""); // Dummy for expand arrow
                } else {
                    m_treeCtrl->AppendItem(
                        parentItem, filename, -1, -1, new PathData(fullPath));
                }
            }
            cont = dir.GetNext(&filename);
        }
        m_treeCtrl->SortChildren(parentItem);
    }

    void OnItemActivated(wxTreeEvent& event) {
        wxTreeItemId itemId = event.GetItem();
        PathData* data = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
        if (!data) return;
        
        wxString path = data->GetPath();
        if (!wxDir::Exists(path) && wxFileExists(path)) {
            // Open file in editor
            if (m_context) {
                auto* editor = m_context->Get<Editor>("editorComponent");
                if (editor) {
                    editor->OpenFile(path);
                }
            }
        }
    }

    void OnItemExpanding(wxTreeEvent& event) {
        wxTreeItemId itemId = event.GetItem();
        wxTreeItemIdValue cookie;
        wxTreeItemId firstChild = m_treeCtrl->GetFirstChild(itemId, cookie);
        
        if (firstChild.IsOk()) {
            PathData* data = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(firstChild));
            if (!data) {
                m_treeCtrl->Delete(firstChild);
                PathData* parentData = dynamic_cast<PathData*>(m_treeCtrl->GetItemData(itemId));
                if (parentData) {
                    PopulateTree(parentData->GetPath(), itemId);
                }
            }
        }
    }
};

// ============================================================================
// EditorWidget - Main code editor
// ============================================================================

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

// ============================================================================
// TerminalWidget - Integrated terminal
// ============================================================================

/**
 * Terminal widget.
 * Provides an integrated command-line interface.
 */
class TerminalWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.terminal";
        info.name = "Terminal";
        info.description = "Integrated command-line terminal";
        info.location = WidgetLocation::Panel;
        info.priority = 100;
        info.showByDefault = false;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_terminal = new Terminal(parent);
        return m_terminal;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        if (m_terminal) {
            m_terminal->ApplyTheme(ThemeManager::Instance().GetCurrentTheme());
        }
    }

    void OnFocus(wxWindow* window, WidgetContext& context) override {
        if (m_terminal) {
            m_terminal->SetFocus();
        }
    }

    void OnShow(wxWindow* window, WidgetContext& context) override {
        if (m_terminal) {
            m_terminal->SetFocus();
        }
    }

    Terminal* GetTerminal() { return m_terminal; }

    std::vector<wxString> GetCommands() const override {
        return {
            "terminal.toggle", "terminal.show", "terminal.hide",
            "terminal.clear", "terminal.focus"
        };
    }

private:
    Terminal* m_terminal = nullptr;
};

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
}

} // namespace BuiltinWidgets

#endif // BUILTIN_WIDGETS_H
