#ifndef WIDGET_ACTIVITY_BAR_H
#define WIDGET_ACTIVITY_BAR_H

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/dcgraph.h>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include "widget.h"
#include "../theme/theme.h"

/**
 * A button in the activity bar representing a widget category.
 */
class ActivityBarButton : public wxPanel {
public:
    ActivityBarButton(wxWindow* parent, const WidgetCategory& category);
    
    const WidgetCategory& GetCategory() const { return m_category; }
    
    void SetSelected(bool selected);
    bool IsSelected() const { return m_selected; }
    
    void SetBadgeCount(int count);
    int GetBadgeCount() const { return m_badgeCount; }
    
    void ApplyTheme(const ThemePtr& theme);
    
    // Callback when button is clicked
    std::function<void(const wxString& categoryId)> OnClick;

private:
    WidgetCategory m_category;
    bool m_selected;
    bool m_hovered;
    int m_badgeCount;
    
    wxBitmap m_iconBitmap;  // Cached icon bitmap
    
    wxColour m_bgColor;
    wxColour m_fgColor;
    wxColour m_selectedColor;
    wxColour m_hoverColor;
    wxColour m_accentColor;
    
    void LoadIcon();
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    
    static const int BUTTON_SIZE = 48;
    static const int ICON_SIZE = 24;
};

/**
 * The activity bar - a vertical strip of category buttons.
 * Similar to VS Code's activity bar on the far left.
 */
class WidgetActivityBar : public wxPanel {
public:
    WidgetActivityBar(wxWindow* parent);
    ~WidgetActivityBar();
    
    // Add a category to the bar
    void AddCategory(const WidgetCategory& category);
    
    // Remove a category
    void RemoveCategory(const wxString& categoryId);
    
    // Select a category (highlight its button)
    void SelectCategory(const wxString& categoryId);
    
    // Get the currently selected category ID
    wxString GetSelectedCategory() const { return m_selectedCategoryId; }
    
    // Check if a category exists
    bool HasCategory(const wxString& categoryId) const;
    
    // Update badge count for a category (e.g., notification count)
    void SetBadgeCount(const wxString& categoryId, int count);
    
    // Apply theme to all buttons
    void ApplyTheme(const ThemePtr& theme);
    
    // Callback when a category is selected
    std::function<void(const wxString& categoryId)> OnCategorySelected;

private:
    std::vector<ActivityBarButton*> m_buttons;
    std::map<wxString, ActivityBarButton*> m_buttonMap;
    wxString m_selectedCategoryId;
    wxBoxSizer* m_mainSizer;
    ThemePtr m_currentTheme;
    
    void RebuildLayout();
    void OnPaint(wxPaintEvent& event);
    
    static const int BAR_WIDTH = 48;
};

#endif // WIDGET_ACTIVITY_BAR_H
