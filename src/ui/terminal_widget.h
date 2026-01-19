#ifndef TERMINAL_WIDGET_H
#define TERMINAL_WIDGET_H

#include "widget.h"
#include "terminal.h"
#include "../theme/theme.h"

namespace BuiltinWidgets {

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

} // namespace BuiltinWidgets

#endif // TERMINAL_WIDGET_H
