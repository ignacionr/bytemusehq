#include "widget_activity_bar.h"
#include <algorithm>

// ============================================================================
// ActivityBarButton Implementation
// ============================================================================

ActivityBarButton::ActivityBarButton(wxWindow* parent, const WidgetCategory& category)
    : wxPanel(parent, wxID_ANY)
    , m_category(category)
    , m_selected(false)
    , m_hovered(false)
    , m_badgeCount(0)
    , m_bgColor(45, 45, 45)
    , m_fgColor(150, 150, 150)
    , m_selectedColor(255, 255, 255)
    , m_hoverColor(200, 200, 200)
    , m_accentColor(0, 122, 204)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(BUTTON_SIZE, BUTTON_SIZE));
    SetMaxSize(wxSize(BUTTON_SIZE, BUTTON_SIZE));
    
    SetToolTip(category.name);
    
    Bind(wxEVT_PAINT, &ActivityBarButton::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ActivityBarButton::OnMouseDown, this);
    Bind(wxEVT_ENTER_WINDOW, &ActivityBarButton::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &ActivityBarButton::OnMouseLeave, this);
}

void ActivityBarButton::SetSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        Refresh();
    }
}

void ActivityBarButton::SetBadgeCount(int count)
{
    if (m_badgeCount != count) {
        m_badgeCount = count;
        Refresh();
    }
}

void ActivityBarButton::ApplyTheme(const ThemePtr& theme)
{
    if (!theme) return;
    
    m_bgColor = theme->ui.activityBarBackground.IsOk() ? 
        theme->ui.activityBarBackground : theme->ui.sidebarBackground.IsOk() ?
        theme->ui.sidebarBackground.ChangeLightness(90) : wxColour(45, 45, 45);
    m_fgColor = theme->ui.activityBarForeground.IsOk() ?
        theme->ui.activityBarForeground : wxColour(150, 150, 150);
    m_selectedColor = theme->ui.foreground.IsOk() ?
        theme->ui.foreground : wxColour(255, 255, 255);
    m_hoverColor = m_fgColor.ChangeLightness(130);
    m_accentColor = theme->ui.accent.IsOk() ?
        theme->ui.accent : wxColour(0, 122, 204);
    
    Refresh();
}

void ActivityBarButton::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    wxSize size = GetSize();
    
    // Background
    dc.SetBackground(wxBrush(m_bgColor));
    dc.Clear();
    
    // Selection indicator (left edge bar)
    if (m_selected) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_accentColor));
        dc.DrawRectangle(0, 0, 3, size.y);
    }
    
    // Hover highlight
    if (m_hovered && !m_selected) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_bgColor.ChangeLightness(120)));
        dc.DrawRectangle(3, 2, size.x - 5, size.y - 4);
    }
    
    // Icon
    wxColour iconColor = m_selected ? m_selectedColor : (m_hovered ? m_hoverColor : m_fgColor);
    dc.SetTextForeground(iconColor);
    
    wxFont iconFont(ICON_SIZE, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    dc.SetFont(iconFont);
    
    wxSize textSize = dc.GetTextExtent(m_category.icon);
    int x = (size.x - textSize.x) / 2;
    int y = (size.y - textSize.y) / 2;
    dc.DrawText(m_category.icon, x, y);
    
    // Badge (if any)
    if (m_badgeCount > 0) {
        wxString badgeText = m_badgeCount > 99 ? "99+" : wxString::Format("%d", m_badgeCount);
        wxFont badgeFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(badgeFont);
        
        wxSize badgeSize = dc.GetTextExtent(badgeText);
        int badgeW = std::max(14, badgeSize.x + 6);
        int badgeH = 14;
        int badgeX = size.x - badgeW - 4;
        int badgeY = 4;
        
        // Badge background
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(wxColour(220, 50, 50)));
        dc.DrawRoundedRectangle(badgeX, badgeY, badgeW, badgeH, 3);
        
        // Badge text
        dc.SetTextForeground(*wxWHITE);
        dc.DrawText(badgeText, badgeX + (badgeW - badgeSize.x) / 2, badgeY + 1);
    }
}

void ActivityBarButton::OnMouseDown(wxMouseEvent& event)
{
    if (OnClick) {
        OnClick(m_category.id);
    }
}

void ActivityBarButton::OnMouseEnter(wxMouseEvent& event)
{
    m_hovered = true;
    Refresh();
}

void ActivityBarButton::OnMouseLeave(wxMouseEvent& event)
{
    m_hovered = false;
    Refresh();
}

// ============================================================================
// WidgetActivityBar Implementation
// ============================================================================

WidgetActivityBar::WidgetActivityBar(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(BAR_WIDTH, -1));
    SetMaxSize(wxSize(BAR_WIDTH, -1));
    
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_mainSizer);
    
    Bind(wxEVT_PAINT, &WidgetActivityBar::OnPaint, this);
}

WidgetActivityBar::~WidgetActivityBar()
{
    m_buttons.clear();
    m_buttonMap.clear();
}

void WidgetActivityBar::AddCategory(const WidgetCategory& category)
{
    // Don't add duplicates
    if (m_buttonMap.find(category.id) != m_buttonMap.end()) {
        return;
    }
    
    ActivityBarButton* button = new ActivityBarButton(this, category);
    button->OnClick = [this](const wxString& categoryId) {
        SelectCategory(categoryId);
        if (OnCategorySelected) {
            OnCategorySelected(categoryId);
        }
    };
    
    if (m_currentTheme) {
        button->ApplyTheme(m_currentTheme);
    }
    
    m_buttons.push_back(button);
    m_buttonMap[category.id] = button;
    
    RebuildLayout();
}

void WidgetActivityBar::RemoveCategory(const wxString& categoryId)
{
    auto it = m_buttonMap.find(categoryId);
    if (it == m_buttonMap.end()) return;
    
    ActivityBarButton* button = it->second;
    m_buttonMap.erase(it);
    
    auto btnIt = std::find(m_buttons.begin(), m_buttons.end(), button);
    if (btnIt != m_buttons.end()) {
        m_buttons.erase(btnIt);
    }
    
    button->Destroy();
    
    // If we removed the selected category, select the first one
    if (m_selectedCategoryId == categoryId && !m_buttons.empty()) {
        SelectCategory(m_buttons[0]->GetCategory().id);
    }
    
    RebuildLayout();
}

void WidgetActivityBar::SelectCategory(const wxString& categoryId)
{
    if (m_selectedCategoryId == categoryId) return;
    
    // Deselect old
    if (!m_selectedCategoryId.empty()) {
        auto it = m_buttonMap.find(m_selectedCategoryId);
        if (it != m_buttonMap.end()) {
            it->second->SetSelected(false);
        }
    }
    
    // Select new
    m_selectedCategoryId = categoryId;
    auto it = m_buttonMap.find(categoryId);
    if (it != m_buttonMap.end()) {
        it->second->SetSelected(true);
    }
}

bool WidgetActivityBar::HasCategory(const wxString& categoryId) const
{
    return m_buttonMap.find(categoryId) != m_buttonMap.end();
}

void WidgetActivityBar::SetBadgeCount(const wxString& categoryId, int count)
{
    auto it = m_buttonMap.find(categoryId);
    if (it != m_buttonMap.end()) {
        it->second->SetBadgeCount(count);
    }
}

void WidgetActivityBar::ApplyTheme(const ThemePtr& theme)
{
    m_currentTheme = theme;
    if (!theme) return;
    
    SetBackgroundColour(theme->ui.activityBarBackground.IsOk() ?
        theme->ui.activityBarBackground : theme->ui.sidebarBackground.IsOk() ?
        theme->ui.sidebarBackground.ChangeLightness(90) : wxColour(45, 45, 45));
    
    for (auto* button : m_buttons) {
        button->ApplyTheme(theme);
    }
    
    Refresh();
}

void WidgetActivityBar::RebuildLayout()
{
    m_mainSizer->Clear(false);
    
    // Sort buttons by category order
    std::vector<ActivityBarButton*> sorted = m_buttons;
    std::sort(sorted.begin(), sorted.end(), [](ActivityBarButton* a, ActivityBarButton* b) {
        return a->GetCategory().order < b->GetCategory().order;
    });
    
    // Add buttons in order
    for (auto* button : sorted) {
        m_mainSizer->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
    }
    
    // Add spacer at bottom
    m_mainSizer->AddStretchSpacer();
    
    Layout();
}

void WidgetActivityBar::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    
    // Draw right border
    wxSize size = GetSize();
    if (m_currentTheme && m_currentTheme->ui.border.IsOk()) {
        dc.SetPen(wxPen(m_currentTheme->ui.border, 1));
    } else {
        dc.SetPen(wxPen(wxColour(60, 60, 60), 1));
    }
    dc.DrawLine(size.x - 1, 0, size.x - 1, size.y);
}
