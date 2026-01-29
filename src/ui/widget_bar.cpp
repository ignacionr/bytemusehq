#include "widget_bar.h"
#include <wx/dcbuffer.h>
#include <algorithm>

// ============================================================================
// WidgetContainer Implementation
// ============================================================================

WidgetContainer::WidgetContainer(wxWindow* parent, const wxString& widgetId, const wxString& title)
    : wxPanel(parent, wxID_ANY)
    , m_widgetId(widgetId)
    , m_content(nullptr)
    , m_collapsed(false)
    , m_heightProportion(1.0)
    , m_lastExpandedHeight(150)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Create header panel
    m_header = new wxPanel(this, wxID_ANY);
    m_header->SetMinSize(wxSize(-1, HEADER_HEIGHT));
    m_header->SetMaxSize(wxSize(-1, HEADER_HEIGHT));
    
    wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Collapse button
    m_collapseBtn = new wxButton(m_header, wxID_ANY, "▼", 
        wxDefaultPosition, wxSize(20, 20), wxBORDER_NONE | wxBU_EXACTFIT);
    m_collapseBtn->SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    m_collapseBtn->Bind(wxEVT_BUTTON, &WidgetContainer::OnCollapseButton, this);
    
    // Title label
    m_titleLabel = new wxStaticText(m_header, wxID_ANY, title);
    m_titleLabel->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    
    headerSizer->Add(m_collapseBtn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    headerSizer->Add(m_titleLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    m_header->SetSizer(headerSizer);
    
    // Double-click on header to collapse/expand
    m_header->Bind(wxEVT_LEFT_DCLICK, &WidgetContainer::OnHeaderDoubleClick, this);
    m_titleLabel->Bind(wxEVT_LEFT_DCLICK, &WidgetContainer::OnHeaderDoubleClick, this);
    
    m_mainSizer->Add(m_header, 0, wxEXPAND);
    
    SetSizer(m_mainSizer);
    
    Bind(wxEVT_PAINT, &WidgetContainer::OnPaint, this);
}

WidgetContainer::~WidgetContainer()
{
    // Content is owned by us through the sizer, will be destroyed automatically
}

void WidgetContainer::SetContent(wxWindow* content)
{
    if (m_content) {
        m_mainSizer->Detach(m_content);
        m_content->Destroy();
    }
    
    m_content = content;
    if (m_content) {
        m_content->Reparent(this);
        m_mainSizer->Add(m_content, 1, wxEXPAND);
        m_content->Show(!m_collapsed);
    }
    
    Layout();
}

void WidgetContainer::SetCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;
    
    if (!collapsed && m_collapsed) {
        // Expanding - remember current height
        m_lastExpandedHeight = GetSize().GetHeight();
    }
    
    m_collapsed = collapsed;
    m_collapseBtn->SetLabel(collapsed ? "▶" : "▼");
    
    if (m_content) {
        m_content->Show(!collapsed);
    }
    
    // Notify parent to rebuild layout
    wxWindow* parent = GetParent();
    if (parent) {
        WidgetBar* bar = dynamic_cast<WidgetBar*>(parent);
        if (bar) {
            bar->RebuildLayout();
        }
    }
}

int WidgetContainer::GetMinimumHeight() const
{
    if (m_collapsed) {
        return HEADER_HEIGHT;
    }
    return HEADER_HEIGHT + MIN_CONTENT_HEIGHT;
}

void WidgetContainer::ApplyTheme(const ThemePtr& theme)
{
    if (!theme) return;
    
    const auto& ui = theme->ui;
    
    SetBackgroundColour(ui.sidebarBackground);
    
    m_header->SetBackgroundColour(ui.sidebarBackground);
    m_titleLabel->SetForegroundColour(ui.sidebarForeground);
    
    m_collapseBtn->SetBackgroundColour(ui.sidebarBackground);
    m_collapseBtn->SetForegroundColour(ui.sidebarForeground);
    
    Refresh();
}

void WidgetContainer::OnCollapseButton(wxCommandEvent& event)
{
    ToggleCollapsed();
}

void WidgetContainer::OnHeaderDoubleClick(wxMouseEvent& event)
{
    ToggleCollapsed();
}

void WidgetContainer::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    
    // Draw a subtle border at the top
    wxSize size = GetSize();
    dc.SetPen(wxPen(GetBackgroundColour().ChangeLightness(80), 1));
    dc.DrawLine(0, 0, size.x, 0);
}

// ============================================================================
// WidgetSash Implementation
// ============================================================================

WidgetSash::WidgetSash(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
    , m_above(nullptr)
    , m_below(nullptr)
    , m_dragging(false)
    , m_dragStartY(0)
    , m_aboveStartHeight(0)
    , m_belowStartHeight(0)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(-1, SASH_HEIGHT));
    SetMaxSize(wxSize(-1, SASH_HEIGHT));
    
    Bind(wxEVT_LEFT_DOWN, &WidgetSash::OnMouseDown, this);
    Bind(wxEVT_LEFT_UP, &WidgetSash::OnMouseUp, this);
    Bind(wxEVT_MOTION, &WidgetSash::OnMouseMove, this);
    Bind(wxEVT_ENTER_WINDOW, &WidgetSash::OnMouseEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &WidgetSash::OnMouseLeave, this);
    Bind(wxEVT_PAINT, &WidgetSash::OnPaint, this);
}

void WidgetSash::SetContainers(WidgetContainer* above, WidgetContainer* below)
{
    m_above = above;
    m_below = below;
}

void WidgetSash::ApplyTheme(const ThemePtr& theme)
{
    if (!theme) return;
    SetBackgroundColour(theme->ui.border);
    Refresh();
}

void WidgetSash::OnMouseDown(wxMouseEvent& event)
{
    if (!m_above || !m_below) return;
    
    // Don't allow resizing if either container is collapsed
    if (m_above->IsCollapsed() || m_below->IsCollapsed()) return;
    
    m_dragging = true;
    m_dragStartY = event.GetY() + GetPosition().y;
    m_aboveStartHeight = m_above->GetSize().GetHeight();
    m_belowStartHeight = m_below->GetSize().GetHeight();
    
    CaptureMouse();
}

void WidgetSash::OnMouseUp(wxMouseEvent& event)
{
    if (m_dragging) {
        m_dragging = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
    }
}

void WidgetSash::OnMouseMove(wxMouseEvent& event)
{
    if (!m_dragging || !m_above || !m_below) return;
    
    int currentY = event.GetY() + GetPosition().y;
    int delta = currentY - m_dragStartY;
    
    int newAboveHeight = m_aboveStartHeight + delta;
    int newBelowHeight = m_belowStartHeight - delta;
    
    // Enforce minimum heights
    int minAbove = m_above->GetMinimumHeight();
    int minBelow = m_below->GetMinimumHeight();
    
    if (newAboveHeight < minAbove) {
        delta = minAbove - m_aboveStartHeight;
        newAboveHeight = minAbove;
        newBelowHeight = m_belowStartHeight - delta;
    }
    if (newBelowHeight < minBelow) {
        delta = m_belowStartHeight - minBelow;
        newBelowHeight = minBelow;
        newAboveHeight = m_aboveStartHeight + delta;
    }
    
    // Apply new sizes
    m_above->SetMinSize(wxSize(-1, newAboveHeight));
    m_above->SetSize(wxSize(-1, newAboveHeight));
    m_below->SetMinSize(wxSize(-1, newBelowHeight));
    m_below->SetSize(wxSize(-1, newBelowHeight));
    
    // Force parent layout
    wxWindow* parent = GetParent();
    if (parent) {
        parent->Layout();
        parent->Refresh();
    }
}

void WidgetSash::OnMouseEnter(wxMouseEvent& event)
{
    if (m_above && m_below && !m_above->IsCollapsed() && !m_below->IsCollapsed()) {
        SetCursor(wxCursor(wxCURSOR_SIZENS));
    }
}

void WidgetSash::OnMouseLeave(wxMouseEvent& event)
{
    if (!m_dragging) {
        SetCursor(wxNullCursor);
    }
}

void WidgetSash::OnPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    
    // Draw grip dots in the center
    wxSize size = GetSize();
    int centerX = size.x / 2;
    int centerY = size.y / 2;
    
    wxColour gripColor = GetBackgroundColour().ChangeLightness(130);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(gripColor));
    
    // Three small dots
    for (int i = -1; i <= 1; ++i) {
        dc.DrawCircle(centerX + i * 8, centerY, 1);
    }
}

// ============================================================================
// WidgetBar Implementation
// ============================================================================

WidgetBar::WidgetBar(wxWindow* parent, WidgetContext& context)
    : wxPanel(parent, wxID_ANY)
    , m_context(context)
{
    m_mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_mainSizer);
    
    Bind(wxEVT_SIZE, &WidgetBar::OnSize, this);
}

WidgetBar::~WidgetBar()
{
    // Containers and sashes are children of this panel,
    // they will be destroyed automatically by wxWidgets
    m_containers.clear();
    m_sashes.clear();
    m_widgets.clear();
}

void WidgetBar::AddWidget(WidgetPtr widget)
{
    if (!widget) return;
    
    auto info = widget->GetInfo();
    wxString categoryId = info.category.id;
    if (categoryId.empty()) {
        categoryId = "tools"; // Default category
    }
    
    m_widgets[info.id] = widget;
    
    // Get or create the widget order list for this category
    auto& widgetOrder = m_widgetOrderByCategory[categoryId];
    
    // Insert in order by priority (higher priority first)
    auto it = std::find_if(widgetOrder.begin(), widgetOrder.end(),
        [this, &info](const wxString& id) {
            auto wit = m_widgets.find(id);
            if (wit != m_widgets.end()) {
                return wit->second->GetInfo().priority < info.priority;
            }
            return false;
        });
    widgetOrder.insert(it, info.id);
    
    // If widget should show by default, mark it visible in its category
    if (info.showByDefault) {
        m_visibleWidgetsByCategory[categoryId].insert(info.id);
    }
    
    // If no active category set, use the first one we add
    if (m_activeCategoryId.empty()) {
        m_activeCategoryId = categoryId;
    }
}

void WidgetBar::SetActiveCategory(const wxString& categoryId)
{
    if (m_activeCategoryId == categoryId) return;
    
    m_activeCategoryId = categoryId;
    RebuildLayout();
}

std::vector<WidgetCategory> WidgetBar::GetCategories() const
{
    std::set<wxString> seenCategories;
    std::vector<WidgetCategory> categories;
    
    for (const auto& [id, widget] : m_widgets) {
        auto info = widget->GetInfo();
        wxString categoryId = info.category.id;
        if (categoryId.empty()) {
            categoryId = "tools";
        }
        
        if (seenCategories.find(categoryId) == seenCategories.end()) {
            seenCategories.insert(categoryId);
            if (info.category.id.empty()) {
                // Use default Tools category
                categories.push_back(WidgetCategories::Tools());
            } else {
                categories.push_back(info.category);
            }
        }
    }
    
    // Sort by order
    std::sort(categories.begin(), categories.end());
    
    return categories;
}

std::vector<wxString> WidgetBar::GetWidgetsInCategory(const wxString& categoryId) const
{
    std::vector<wxString> result;
    
    for (const auto& [id, widget] : m_widgets) {
        auto info = widget->GetInfo();
        wxString widgetCategory = info.category.id;
        if (widgetCategory.empty()) {
            widgetCategory = "tools";
        }
        
        if (widgetCategory == categoryId) {
            result.push_back(id);
        }
    }
    
    return result;
}

WidgetContainer* WidgetBar::GetOrCreateContainer(const wxString& widgetId)
{
    auto it = m_containers.find(widgetId);
    if (it != m_containers.end()) {
        return it->second;
    }
    
    auto wit = m_widgets.find(widgetId);
    if (wit == m_widgets.end()) {
        return nullptr;
    }
    
    auto& widget = wit->second;
    auto info = widget->GetInfo();
    
    // Create container
    WidgetContainer* container = new WidgetContainer(this, widgetId, info.name);
    
    // Create widget content
    wxWindow* content = widget->CreateWindow(container, m_context);
    if (content) {
        container->SetContent(content);
        widget->RegisterCommands(m_context);
    }
    
    // Apply current theme
    if (m_currentTheme) {
        container->ApplyTheme(m_currentTheme);
        widget->OnThemeChanged(content, m_context);
    }
    
    m_containers[widgetId] = container;
    container->Hide(); // Start hidden, RebuildLayout will show if needed
    
    return container;
}

void WidgetBar::ShowWidget(const wxString& widgetId, bool show)
{
    auto wit = m_widgets.find(widgetId);
    if (wit == m_widgets.end()) return;
    
    auto info = wit->second->GetInfo();
    wxString categoryId = info.category.id;
    if (categoryId.empty()) {
        categoryId = "tools";
    }
    
    auto& visibleSet = m_visibleWidgetsByCategory[categoryId];
    bool currentlyVisible = visibleSet.count(widgetId) > 0;
    if (currentlyVisible == show) return;
    
    if (show) {
        visibleSet.insert(widgetId);
    } else {
        visibleSet.erase(widgetId);
    }
    
    // Only rebuild if this affects the active category
    if (categoryId == m_activeCategoryId) {
        RebuildLayout();
    }
}

void WidgetBar::ToggleWidget(const wxString& widgetId)
{
    ShowWidget(widgetId, !IsWidgetVisible(widgetId));
}

bool WidgetBar::IsWidgetVisible(const wxString& widgetId) const
{
    auto wit = m_widgets.find(widgetId);
    if (wit == m_widgets.end()) return false;
    
    auto info = wit->second->GetInfo();
    wxString categoryId = info.category.id;
    if (categoryId.empty()) {
        categoryId = "tools";
    }
    
    auto it = m_visibleWidgetsByCategory.find(categoryId);
    if (it == m_visibleWidgetsByCategory.end()) return false;
    
    return it->second.count(widgetId) > 0;
}

bool WidgetBar::HasVisibleWidgets() const
{
    auto it = m_visibleWidgetsByCategory.find(m_activeCategoryId);
    if (it == m_visibleWidgetsByCategory.end()) return false;
    return !it->second.empty();
}

std::vector<wxString> WidgetBar::GetVisibleWidgetIds() const
{
    std::vector<wxString> result;
    
    auto orderIt = m_widgetOrderByCategory.find(m_activeCategoryId);
    auto visibleIt = m_visibleWidgetsByCategory.find(m_activeCategoryId);
    
    if (orderIt == m_widgetOrderByCategory.end() || 
        visibleIt == m_visibleWidgetsByCategory.end()) {
        return result;
    }
    
    for (const auto& id : orderIt->second) {
        if (visibleIt->second.count(id)) {
            result.push_back(id);
        }
    }
    
    return result;
}

void WidgetBar::ApplyTheme(const ThemePtr& theme)
{
    m_currentTheme = theme;
    if (!theme) return;
    
    SetBackgroundColour(theme->ui.sidebarBackground);
    
    for (auto& [id, container] : m_containers) {
        container->ApplyTheme(theme);
    }
    
    for (auto* sash : m_sashes) {
        sash->ApplyTheme(theme);
    }
    
    Refresh();
}

void WidgetBar::NotifyThemeChanged()
{
    for (auto& [id, widget] : m_widgets) {
        auto it = m_containers.find(id);
        if (it != m_containers.end() && it->second) {
            wxWindow* content = it->second->GetContent();
            if (content) {
                widget->OnThemeChanged(content, m_context);
            }
        }
    }
}

void WidgetBar::RebuildLayout()
{
    // Detach everything from sizer (don't destroy)
    m_mainSizer->Clear(false);
    
    // Hide all containers first
    for (auto& [id, container] : m_containers) {
        container->Hide();
    }
    
    // Destroy old sashes
    for (auto* sash : m_sashes) {
        sash->Destroy();
    }
    m_sashes.clear();
    
    // Get visible containers in order for the active category
    std::vector<WidgetContainer*> visibleContainers;
    
    auto orderIt = m_widgetOrderByCategory.find(m_activeCategoryId);
    auto visibleIt = m_visibleWidgetsByCategory.find(m_activeCategoryId);
    
    if (orderIt != m_widgetOrderByCategory.end() && 
        visibleIt != m_visibleWidgetsByCategory.end()) {
        for (const auto& id : orderIt->second) {
            if (visibleIt->second.count(id)) {
                WidgetContainer* container = GetOrCreateContainer(id);
                if (container) {
                    visibleContainers.push_back(container);
                }
            }
        }
    }
    
    if (visibleContainers.empty()) {
        Layout();
        return;
    }
    
    // Build layout with sashes between containers
    for (size_t i = 0; i < visibleContainers.size(); ++i) {
        WidgetContainer* container = visibleContainers[i];
        
        // Add container
        container->Show();
        
        // Reset min size so sizer can work properly
        container->SetMinSize(wxSize(-1, container->GetMinimumHeight()));
        
        // Give each visible non-collapsed container equal proportion
        int proportion = container->IsCollapsed() ? 0 : 1;
        m_mainSizer->Add(container, proportion, wxEXPAND);
        
        // Add sash after each container except the last
        if (i < visibleContainers.size() - 1) {
            WidgetSash* sash = new WidgetSash(this);
            sash->SetContainers(container, visibleContainers[i + 1]);
            if (m_currentTheme) {
                sash->ApplyTheme(m_currentTheme);
            }
            m_sashes.push_back(sash);
            m_mainSizer->Add(sash, 0, wxEXPAND);
        }
    }
    
    Layout();
    Refresh();
}

void WidgetBar::DistributeHeight()
{
    // This is called after resize to distribute height among visible widgets
    // For now, the sizer handles this automatically with equal proportions
}

void WidgetBar::OnSize(wxSizeEvent& event)
{
    event.Skip();
    DistributeHeight();
}
