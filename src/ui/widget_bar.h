#ifndef WIDGET_BAR_H
#define WIDGET_BAR_H

#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "widget.h"
#include "../theme/theme.h"

/**
 * A container panel for a single widget in the widget bar.
 * Includes a header with the widget name and a collapsible content area.
 */
class WidgetContainer : public wxPanel {
public:
    WidgetContainer(wxWindow* parent, const wxString& widgetId, const wxString& title);
    ~WidgetContainer();
    
    // Set the actual widget content
    void SetContent(wxWindow* content);
    wxWindow* GetContent() const { return m_content; }
    
    // Collapse/expand the content (header stays visible)
    void SetCollapsed(bool collapsed);
    bool IsCollapsed() const { return m_collapsed; }
    void ToggleCollapsed() { SetCollapsed(!m_collapsed); }
    
    // Get the widget ID this container holds
    const wxString& GetWidgetId() const { return m_widgetId; }
    
    // Get minimum height (header only when collapsed, header + min content when expanded)
    int GetMinimumHeight() const;
    
    // Set the expanded height proportion (0.0 to 1.0)
    void SetHeightProportion(double proportion) { m_heightProportion = proportion; }
    double GetHeightProportion() const { return m_heightProportion; }
    
    // Apply theme colors
    void ApplyTheme(const ThemePtr& theme);

private:
    wxString m_widgetId;
    wxPanel* m_header;
    wxStaticText* m_titleLabel;
    wxButton* m_collapseBtn;
    wxWindow* m_content;
    wxBoxSizer* m_mainSizer;
    bool m_collapsed;
    double m_heightProportion;
    int m_lastExpandedHeight;
    
    void OnCollapseButton(wxCommandEvent& event);
    void OnHeaderDoubleClick(wxMouseEvent& event);
    void OnPaint(wxPaintEvent& event);
    
    static const int HEADER_HEIGHT = 24;
    static const int MIN_CONTENT_HEIGHT = 60;
};

/**
 * A resizable sash between widget containers.
 */
class WidgetSash : public wxPanel {
public:
    WidgetSash(wxWindow* parent);
    
    void SetContainers(WidgetContainer* above, WidgetContainer* below);
    void ApplyTheme(const ThemePtr& theme);

private:
    WidgetContainer* m_above;
    WidgetContainer* m_below;
    bool m_dragging;
    int m_dragStartY;
    int m_aboveStartHeight;
    int m_belowStartHeight;
    
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnPaint(wxPaintEvent& event);
    
    static const int SASH_HEIGHT = 4;
};

/**
 * The main widget bar that manages multiple widgets in a vertical layout.
 * 
 * Features:
 * - Widgets can be shown/hidden dynamically
 * - Widgets can be collapsed to show only their header
 * - Resizable sashes between widgets for manual height adjustment
 * - Category filtering to show only widgets from selected category
 * - Proper cleanup when hiding widgets (no reparenting issues)
 * - Theme support
 */
class WidgetBar : public wxPanel {
public:
    WidgetBar(wxWindow* parent, WidgetContext& context);
    ~WidgetBar();
    
    // Add a widget to the bar (creates container but doesn't show yet)
    void AddWidget(WidgetPtr widget);
    
    // Category filtering
    void SetActiveCategory(const wxString& categoryId);
    wxString GetActiveCategory() const { return m_activeCategoryId; }
    
    // Get all unique categories from registered widgets
    std::vector<WidgetCategory> GetCategories() const;
    
    // Get widgets in a specific category
    std::vector<wxString> GetWidgetsInCategory(const wxString& categoryId) const;
    
    // Show/hide a specific widget (within its category)
    void ShowWidget(const wxString& widgetId, bool show = true);
    void HideWidget(const wxString& widgetId) { ShowWidget(widgetId, false); }
    void ToggleWidget(const wxString& widgetId);
    
    // Check if a widget is currently visible
    bool IsWidgetVisible(const wxString& widgetId) const;
    
    // Check if any widgets are visible in the active category
    bool HasVisibleWidgets() const;
    
    // Get the list of visible widget IDs in active category
    std::vector<wxString> GetVisibleWidgetIds() const;
    
    // Apply theme to all widgets and containers
    void ApplyTheme(const ThemePtr& theme);
    
    // Notify widgets of theme change
    void NotifyThemeChanged();
    
    // Rebuild the layout (call after show/hide changes)
    void RebuildLayout();

private:
    WidgetContext& m_context;
    
    // All widgets registered with this bar
    std::map<wxString, WidgetPtr> m_widgets;
    
    // Containers for each widget (created on first show)
    std::map<wxString, WidgetContainer*> m_containers;
    
    // Set of visible widget IDs per category
    std::map<wxString, std::set<wxString>> m_visibleWidgetsByCategory;
    
    // Order of widgets within each category (by priority from WidgetInfo)
    std::map<wxString, std::vector<wxString>> m_widgetOrderByCategory;
    
    // Currently active category
    wxString m_activeCategoryId;
    
    // Sashes between visible containers
    std::vector<WidgetSash*> m_sashes;
    
    // Main sizer for the bar
    wxBoxSizer* m_mainSizer;
    
    // Current theme
    ThemePtr m_currentTheme;
    
    // Create a container for a widget if it doesn't exist
    WidgetContainer* GetOrCreateContainer(const wxString& widgetId);
    
    // Clear and rebuild sashes for current visible widgets
    void RebuildSashes();
    
    // Distribute available height among visible widgets
    void DistributeHeight();
    
    void OnSize(wxSizeEvent& event);
};

#endif // WIDGET_BAR_H
