#ifndef JIRA_WIDGET_H
#define JIRA_WIDGET_H

#include "widget.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "../jira/jira_client.h"
#include <wx/dcbuffer.h>
#include <wx/timer.h>
#include <wx/listctrl.h>
#include <wx/hyperlink.h>
#include <wx/simplebook.h>
#include <wx/statline.h>
#include <wx/choice.h>
#include <wx/textctrl.h>
#include <wx/mimetype.h>

#include <thread>
#include <mutex>
#include <atomic>

// Forward declaration
class MainFrame;

namespace BuiltinWidgets {

/**
 * JIRA issue structure for display (UI-specific with wxString).
 * This wraps the Jira::Issue for wxWidgets UI use.
 */
struct JiraIssue {
    wxString key;           // e.g., "PROJ-123"
    wxString summary;       // Issue title
    wxString status;        // "To Do", "In Progress", "Done", etc.
    wxString priority;      // "Highest", "High", "Medium", "Low", "Lowest"
    wxString type;          // "Bug", "Story", "Task", "Epic"
    wxString assignee;      // Assignee display name
    wxString updated;       // Last updated timestamp
    wxString url;           // Web URL to the issue
};

/**
 * Custom panel for displaying a single JIRA issue card with colourful styling.
 */
class JiraIssueCard : public wxPanel {
public:
    JiraIssueCard(wxWindow* parent, const JiraIssue& issue)
        : wxPanel(parent, wxID_ANY)
        , m_issue(issue)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(-1, 70));
        
        Bind(wxEVT_PAINT, &JiraIssueCard::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &JiraIssueCard::OnClick, this);
        Bind(wxEVT_ENTER_WINDOW, &JiraIssueCard::OnMouseEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &JiraIssueCard::OnMouseLeave, this);
    }
    
    void SetThemeColors(const wxColour& bg, const wxColour& fg) {
        m_bgColor = bg;
        m_fgColor = fg;
        Refresh();
    }
    
    const JiraIssue& GetIssue() const { return m_issue; }
    
private:
    JiraIssue m_issue;
    wxColour m_bgColor = wxColour(40, 40, 40);
    wxColour m_fgColor = wxColour(220, 220, 220);
    bool m_hovered = false;
    
    wxColour GetStatusColor() const {
        // Colourful status indicators
        if (m_issue.status.Contains("Done") || m_issue.status.Contains("Closed") || 
            m_issue.status.Contains("Resolved")) {
            return wxColour(46, 204, 113);  // Green - completed
        } else if (m_issue.status.Contains("Progress") || m_issue.status.Contains("Review")) {
            return wxColour(52, 152, 219);  // Blue - in progress
        } else if (m_issue.status.Contains("Block") || m_issue.status.Contains("Impediment")) {
            return wxColour(231, 76, 60);   // Red - blocked
        } else if (m_issue.status.Contains("Testing") || m_issue.status.Contains("QA")) {
            return wxColour(155, 89, 182);  // Purple - testing
        } else {
            return wxColour(149, 165, 166); // Gray - to do/backlog
        }
    }
    
    wxColour GetPriorityColor() const {
        if (m_issue.priority.Contains("Highest") || m_issue.priority.Contains("Blocker")) {
            return wxColour(231, 76, 60);   // Red
        } else if (m_issue.priority.Contains("High") || m_issue.priority.Contains("Critical")) {
            return wxColour(230, 126, 34);  // Orange
        } else if (m_issue.priority.Contains("Medium") || m_issue.priority.Contains("Major")) {
            return wxColour(241, 196, 15);  // Yellow
        } else if (m_issue.priority.Contains("Low") || m_issue.priority.Contains("Minor")) {
            return wxColour(52, 152, 219);  // Blue
        } else {
            return wxColour(149, 165, 166); // Gray - lowest
        }
    }
    
    wxColour GetTypeColor() const {
        if (m_issue.type.Contains("Bug")) {
            return wxColour(231, 76, 60);   // Red
        } else if (m_issue.type.Contains("Story")) {
            return wxColour(46, 204, 113);  // Green
        } else if (m_issue.type.Contains("Epic")) {
            return wxColour(155, 89, 182);  // Purple
        } else if (m_issue.type.Contains("Sub")) {
            return wxColour(52, 152, 219);  // Blue
        } else {
            return wxColour(52, 152, 219);  // Blue - Task default
        }
    }
    
    wxString GetTypeIcon() const {
        if (m_issue.type.Contains("Bug")) return wxT("\U0001F41E"); // 🐞
        if (m_issue.type.Contains("Story")) return wxT("\U0001F4D6"); // 📖
        if (m_issue.type.Contains("Epic")) return wxT("\u26A1"); // ⚡
        if (m_issue.type.Contains("Sub")) return wxT("\U0001F4CB"); // 📋
        return wxT("\u2611"); // ☑ - Task
    }
    
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetClientSize();
        
        // Card background with hover effect
        wxColour cardBg = m_hovered 
            ? wxColour(std::min(255, m_bgColor.Red() + 20),
                      std::min(255, m_bgColor.Green() + 20),
                      std::min(255, m_bgColor.Blue() + 20))
            : wxColour(std::min(255, m_bgColor.Red() + 10),
                      std::min(255, m_bgColor.Green() + 10),
                      std::min(255, m_bgColor.Blue() + 10));
        
        dc.SetBrush(wxBrush(cardBg));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(2, 2, size.GetWidth() - 4, size.GetHeight() - 4, 6);
        
        // Left status bar (colourful vertical strip)
        wxColour statusColor = GetStatusColor();
        dc.SetBrush(wxBrush(statusColor));
        dc.DrawRoundedRectangle(4, 6, 4, size.GetHeight() - 12, 2);
        
        int x = 16;
        int y = 8;
        
        // Type icon + Key
        wxFont keyFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(keyFont);
        dc.SetTextForeground(GetTypeColor());
        dc.DrawText(GetTypeIcon(), x, y);
        x += 22;
        
        dc.SetTextForeground(wxColour(100, 180, 255)); // Blue link color
        dc.DrawText(m_issue.key, x, y);
        
        // Priority indicator (small colored dot)
        wxColour priorityColor = GetPriorityColor();
        dc.SetBrush(wxBrush(priorityColor));
        dc.DrawCircle(size.GetWidth() - 15, y + 6, 5);
        
        y += 18;
        x = 16;
        
        // Summary (truncated if too long)
        wxFont summaryFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        dc.SetFont(summaryFont);
        dc.SetTextForeground(m_fgColor);
        
        wxString summary = m_issue.summary;
        int maxWidth = size.GetWidth() - 24;
        while (dc.GetTextExtent(summary).GetWidth() > maxWidth && summary.Length() > 3) {
            summary = summary.Left(summary.Length() - 4) + "...";
        }
        dc.DrawText(summary, x, y);
        
        y += 18;
        
        // Status badge
        wxFont statusFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(statusFont);
        wxSize statusSize = dc.GetTextExtent(m_issue.status);
        
        dc.SetBrush(wxBrush(statusColor));
        dc.DrawRoundedRectangle(x, y, statusSize.GetWidth() + 10, 16, 3);
        
        dc.SetTextForeground(*wxWHITE);
        dc.DrawText(m_issue.status, x + 5, y + 1);
        
        // Updated time (right aligned, muted)
        wxFont timeFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL);
        dc.SetFont(timeFont);
        dc.SetTextForeground(wxColour(120, 120, 120));
        wxSize timeSize = dc.GetTextExtent(m_issue.updated);
        dc.DrawText(m_issue.updated, size.GetWidth() - timeSize.GetWidth() - 10, y + 1);
    }
    
    void OnClick(wxMouseEvent& event) {
        // Open issue in browser
        if (!m_issue.url.IsEmpty()) {
            wxLaunchDefaultBrowser(m_issue.url);
        }
    }
    
    void OnMouseEnter(wxMouseEvent&) {
        m_hovered = true;
        SetCursor(wxCursor(wxCURSOR_HAND));
        Refresh();
    }
    
    void OnMouseLeave(wxMouseEvent&) {
        m_hovered = false;
        SetCursor(wxCursor(wxCURSOR_ARROW));
        Refresh();
    }
};

/**
 * JIRA widget for tracking assigned issues.
 * Displays issues with colourful status indicators and allows quick capture of new items.
 */
class JiraWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.jira";
        info.name = "JIRA Issues";
        info.description = "View and manage your JIRA issues";
        info.location = WidgetLocation::Sidebar;
        info.category = WidgetCategories::Productivity();
        info.priority = 60;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        m_context = &context;
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // Header with icon
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
#ifdef __WXMSW__
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("[J] JIRA Issues"));
#else
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("\U0001F3AF JIRA Issues")); // 🎯
#endif
        wxFont headerFont = m_headerLabel->GetFont();
        headerFont.SetWeight(wxFONTWEIGHT_BOLD);
        headerFont.SetPointSize(11);
        m_headerLabel->SetFont(headerFont);
        headerSizer->Add(m_headerLabel, 1, wxALIGN_CENTER_VERTICAL);
        
        // Refresh button
#ifdef __WXMSW__
        m_refreshBtn = new wxButton(m_panel, wxID_ANY, wxT("R"), wxDefaultPosition, wxSize(28, 24));
#else
        m_refreshBtn = new wxButton(m_panel, wxID_ANY, wxT("\U0001F504"), wxDefaultPosition, wxSize(28, 24)); // 🔄
#endif
        m_refreshBtn->SetToolTip("Refresh issues");
        headerSizer->Add(m_refreshBtn, 0, wxLEFT, 5);
        
        mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);
        
        // Connection status / config info
        m_statusLabel = new wxStaticText(m_panel, wxID_ANY, "");
        wxFont statusFont = m_statusLabel->GetFont();
        statusFont.SetPointSize(8);
        m_statusLabel->SetFont(statusFont);
        mainSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        
        // Tab buttons for switching views
        wxBoxSizer* tabSizer = new wxBoxSizer(wxHORIZONTAL);
        m_myIssuesBtn = new wxButton(m_panel, wxID_ANY, "My Issues", wxDefaultPosition, wxSize(-1, 26));
        m_createBtn = new wxButton(m_panel, wxID_ANY, "+ New", wxDefaultPosition, wxSize(-1, 26));
        
        tabSizer->Add(m_myIssuesBtn, 1, wxRIGHT, 2);
        tabSizer->Add(m_createBtn, 0);
        mainSizer->Add(tabSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        
        mainSizer->AddSpacer(5);
        
        // Notebook for switching between issues list and create form
        m_book = new wxSimplebook(m_panel);
        
        // === Issues List Page ===
        m_issuesPanel = new wxScrolledWindow(m_book, wxID_ANY);
        m_issuesPanel->SetScrollRate(0, 10);
        m_issuesSizer = new wxBoxSizer(wxVERTICAL);
        m_issuesPanel->SetSizer(m_issuesSizer);
        m_book->AddPage(m_issuesPanel, "Issues");
        
        // === Create Issue Page ===
        wxPanel* createPanel = new wxPanel(m_book);
        wxBoxSizer* createSizer = new wxBoxSizer(wxVERTICAL);
        
        // Project selection
        wxStaticText* projLabel = new wxStaticText(createPanel, wxID_ANY, "Project:");
        m_projectChoice = new wxChoice(createPanel, wxID_ANY);
        m_projectChoice->Append("PROJ");  // Will be populated from config
        m_projectChoice->SetSelection(0);
        createSizer->Add(projLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_projectChoice, 0, wxEXPAND | wxALL, 5);
        
        // Issue type
        wxStaticText* typeLabel = new wxStaticText(createPanel, wxID_ANY, "Type:");
        m_typeChoice = new wxChoice(createPanel, wxID_ANY);
        m_typeChoice->Append(wxT("\U0001F41E Bug"));     // 🐞
        m_typeChoice->Append(wxT("\U0001F4D6 Story"));   // 📖
        m_typeChoice->Append(wxT("\u2611 Task"));        // ☑
        m_typeChoice->Append(wxT("\U0001F4CB Sub-task"));// 📋
        m_typeChoice->SetSelection(2);  // Default to Task
        createSizer->Add(typeLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_typeChoice, 0, wxEXPAND | wxALL, 5);
        
        // Priority
        wxStaticText* prioLabel = new wxStaticText(createPanel, wxID_ANY, "Priority:");
        m_priorityChoice = new wxChoice(createPanel, wxID_ANY);
        m_priorityChoice->Append(wxT("\U0001F534 Highest")); // 🔴
        m_priorityChoice->Append(wxT("\U0001F7E0 High"));    // 🟠
        m_priorityChoice->Append(wxT("\U0001F7E1 Medium"));  // 🟡
        m_priorityChoice->Append(wxT("\U0001F535 Low"));     // 🔵
        m_priorityChoice->Append(wxT("\u26AA Lowest"));      // ⚪
        m_priorityChoice->SetSelection(2);  // Default to Medium
        createSizer->Add(prioLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_priorityChoice, 0, wxEXPAND | wxALL, 5);
        
        // Summary
        wxStaticText* summaryLabel = new wxStaticText(createPanel, wxID_ANY, "Summary:");
        m_summaryText = new wxTextCtrl(createPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
        m_summaryText->SetHint("Enter issue summary...");
        createSizer->Add(summaryLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_summaryText, 0, wxEXPAND | wxALL, 5);
        
        // Description
        wxStaticText* descLabel = new wxStaticText(createPanel, wxID_ANY, "Description:");
        m_descText = new wxTextCtrl(createPanel, wxID_ANY, "", wxDefaultPosition, 
                                     wxSize(-1, 80), wxTE_MULTILINE);
        m_descText->SetHint("Enter description (optional)...");
        createSizer->Add(descLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_descText, 1, wxEXPAND | wxALL, 5);
        
        // Create button
        wxBoxSizer* createBtnSizer = new wxBoxSizer(wxHORIZONTAL);
        createBtnSizer->AddStretchSpacer();
        m_submitBtn = new wxButton(createPanel, wxID_ANY, wxT("\u2795 Create Issue")); // ➕
        createBtnSizer->Add(m_submitBtn, 0, wxALL, 5);
        createSizer->Add(createBtnSizer, 0, wxEXPAND);
        
        createPanel->SetSizer(createSizer);
        m_book->AddPage(createPanel, "Create");
        
        mainSizer->Add(m_book, 1, wxEXPAND | wxALL, 5);
        
        // Legend footer
        m_legendLabel = new wxStaticText(m_panel, wxID_ANY, 
            wxT("\U0001F7E2 Done  \U0001F535 In Progress  \u26AA To Do  \U0001F534 Blocked"));
        wxFont legendFont = m_legendLabel->GetFont();
        legendFont.SetPointSize(7);
        m_legendLabel->SetFont(legendFont);
        mainSizer->Add(m_legendLabel, 0, wxALL | wxALIGN_CENTER, 5);
        
        m_panel->SetSizer(mainSizer);
        
        // Bind events
        m_refreshBtn->Bind(wxEVT_BUTTON, &JiraWidget::OnRefresh, this);
        m_myIssuesBtn->Bind(wxEVT_BUTTON, &JiraWidget::OnShowMyIssues, this);
        m_createBtn->Bind(wxEVT_BUTTON, &JiraWidget::OnShowCreate, this);
        m_submitBtn->Bind(wxEVT_BUTTON, &JiraWidget::OnCreateIssue, this);
        
        // Load configuration and fetch issues from JIRA API
        LoadConfig();
        FetchIssuesFromApi();
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme || !m_panel) return;
        
        m_bgColor = theme->ui.sidebarBackground;
        m_fgColor = theme->ui.sidebarForeground;
        
        m_panel->SetBackgroundColour(m_bgColor);
        m_headerLabel->SetForegroundColour(m_fgColor);
        m_statusLabel->SetForegroundColour(wxColour(120, 120, 120));
        m_issuesPanel->SetBackgroundColour(m_bgColor);
        if (m_legendLabel) {
            m_legendLabel->SetForegroundColour(wxColour(150, 150, 150));
        }
        
        // Update all issue cards
        for (wxWindow* child : m_issuesPanel->GetChildren()) {
            if (auto* card = dynamic_cast<JiraIssueCard*>(child)) {
                card->SetThemeColors(m_bgColor, m_fgColor);
            }
        }
        
        m_panel->Refresh();
    }

    std::vector<wxString> GetCommands() const override {
        return {
            "jira.refresh",
            "jira.show",
            "jira.hide",
            "jira.toggle",
            "jira.create",
            "jira.configure"
        };
    }

    void RegisterCommands(WidgetContext& context) override {
        auto& registry = CommandRegistry::Instance();
        m_context = &context;
        
        auto makeCmd = [](const wxString& id, const wxString& title, 
                         const wxString& desc, Command::ExecuteFunc exec,
                         Command::EnabledFunc enabled = nullptr) {
            auto cmd = std::make_shared<Command>(id, title, "JIRA");
            cmd->SetDescription(desc);
            cmd->SetExecuteHandler(std::move(exec));
            if (enabled) cmd->SetEnabledHandler(std::move(enabled));
            return cmd;
        };
        
        JiraWidget* self = this;
        
        registry.Register(makeCmd(
            "jira.toggle", "Toggle JIRA Widget",
            "Show or hide the JIRA issues widget",
            [self](CommandContext& ctx) {
                self->ToggleVisibility(ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "jira.show", "Show JIRA Widget",
            "Show the JIRA issues widget in the sidebar",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "jira.hide", "Hide JIRA Widget",
            "Hide the JIRA issues widget",
            [self](CommandContext& ctx) {
                self->SetVisible(false, ctx);
            },
            [self](const CommandContext& ctx) {
                return self->IsVisible();
            }
        ));
        
        registry.Register(makeCmd(
            "jira.refresh", "Refresh JIRA Issues",
            "Refresh the list of assigned JIRA issues",
            [self](CommandContext& ctx) {
                self->RefreshIssues();
            }
        ));
        
        registry.Register(makeCmd(
            "jira.create", "Create JIRA Issue",
            "Open the JIRA issue creation form",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->ShowCreateForm();
            }
        ));
        
        registry.Register(makeCmd(
            "jira.configure", "Configure JIRA",
            "Open JIRA settings in config file",
            [self](CommandContext& ctx) {
                self->OpenConfiguration();
            }
        ));
    }
    
    wxWindow* GetWindow() { return m_panel; }
    
    bool IsVisible() const {
        return m_panel && m_panel->IsShown();
    }
    
    void SetVisible(bool visible, CommandContext& ctx) {
        auto* frame = ctx.Get<MainFrame>("mainFrame");
        if (frame) {
            frame->ShowSidebarWidget("core.jira", visible);
        }
    }
    
    void ToggleVisibility(CommandContext& ctx) {
        SetVisible(!IsVisible(), ctx);
    }
    
    void RefreshIssues() {
        if (!m_loading) {
            FetchIssuesFromApi();
        }
    }
    
    void ShowCreateForm() {
        if (m_book) {
            m_book->SetSelection(1);
            m_summaryText->SetFocus();
        }
    }
    
    void OpenConfiguration() {
        // Open config file or show configuration dialog
        auto& config = Config::Instance();
        wxString configDir = config.GetConfigDir();
        wxLaunchDefaultApplication(configDir);
    }

private:
    wxPanel* m_panel = nullptr;
    wxStaticText* m_headerLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxStaticText* m_legendLabel = nullptr;
    wxButton* m_refreshBtn = nullptr;
    wxButton* m_myIssuesBtn = nullptr;
    wxButton* m_createBtn = nullptr;
    wxSimplebook* m_book = nullptr;
    wxScrolledWindow* m_issuesPanel = nullptr;
    wxBoxSizer* m_issuesSizer = nullptr;
    wxChoice* m_projectChoice = nullptr;
    wxChoice* m_typeChoice = nullptr;
    wxChoice* m_priorityChoice = nullptr;
    wxTextCtrl* m_summaryText = nullptr;
    wxTextCtrl* m_descText = nullptr;
    wxButton* m_submitBtn = nullptr;
    WidgetContext* m_context = nullptr;
    
    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);
    
    wxString m_jiraProject;
    
    std::vector<JiraIssue> m_issues;
    std::atomic<bool> m_loading{false};
    wxTimer m_refreshTimer;
    
    // Jira client
    Jira::Client m_jiraClient;
    
    /**
     * Convert Jira::Issue to JiraIssue (wxString-based for UI).
     */
    JiraIssue ConvertIssue(const Jira::Issue& issue) {
        JiraIssue uiIssue;
        uiIssue.key = wxString::FromUTF8(issue.key.c_str());
        uiIssue.summary = wxString::FromUTF8(issue.summary.c_str());
        uiIssue.status = wxString::FromUTF8(issue.status.c_str());
        uiIssue.priority = wxString::FromUTF8(issue.priority.c_str());
        uiIssue.type = wxString::FromUTF8(issue.type.c_str());
        uiIssue.assignee = wxString::FromUTF8(issue.assignee.c_str());
        uiIssue.updated = FormatRelativeTime(wxString::FromUTF8(issue.updated.c_str()));
        uiIssue.url = wxString::FromUTF8(issue.url.c_str());
        return uiIssue;
    }
    
    wxString FormatRelativeTime(const wxString& isoTime) {
        // Parse ISO 8601 timestamp and return relative time
        // Format: 2026-01-19T10:30:00.000+0000
        wxDateTime dt;
        if (!dt.ParseISOCombined(isoTime.BeforeFirst('.'))) {
            return isoTime.Left(10);  // Just return date part
        }
        
        wxDateTime now = wxDateTime::Now();
        wxTimeSpan diff = now - dt;
        
        if (diff.GetMinutes() < 60) {
            return wxString::Format("%dm ago", (int)diff.GetMinutes());
        } else if (diff.GetHours() < 24) {
            return wxString::Format("%dh ago", (int)diff.GetHours());
        } else if (diff.GetDays() < 7) {
            return wxString::Format("%dd ago", (int)diff.GetDays());
        } else {
            return isoTime.Left(10);
        }
    }
    
    void LoadConfig() {
        // Load config and update client
        auto config = Jira::ClientConfig::LoadFromConfig();
        m_jiraClient.SetConfig(config);
        m_jiraProject = wxString::FromUTF8(config.defaultProject.c_str());
        
        // Update status label
        if (!m_jiraClient.IsConfigured()) {
            m_statusLabel->SetLabel(wxT("\u26A0 Configure jira.apiUrl, jira.user, jira.apiToken"));
            m_statusLabel->SetForegroundColour(wxColour(241, 196, 15)); // Yellow warning
        } else {
            m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s"), wxString::FromUTF8(config.user.c_str())));
            m_statusLabel->SetForegroundColour(wxColour(46, 204, 113)); // Green
        }
        
        // Update project choice
        if (m_projectChoice && !m_jiraProject.IsEmpty()) {
            m_projectChoice->Clear();
            m_projectChoice->Append(m_jiraProject);
            m_projectChoice->SetSelection(0);
        }
    }
    
    void FetchIssuesFromApi() {
        if (!m_jiraClient.IsConfigured()) {
            // Show configuration message
            wxTheApp->CallAfter([this]() {
                m_issuesSizer->Clear(true);
                m_issues.clear();
                
                auto* msgLabel = new wxStaticText(m_issuesPanel, wxID_ANY,
                    "Please configure JIRA settings:\n\n"
                    "1. jira.apiUrl\n"
                    "2. jira.user (email)\n"
                    "3. jira.apiToken\n\n"
                    "Get your API token from:\n"
                    "https://id.atlassian.com/manage-profile/security/api-tokens");
                msgLabel->SetForegroundColour(wxColour(180, 180, 180));
                msgLabel->Wrap(200);
                m_issuesSizer->Add(msgLabel, 0, wxALL, 10);
                
                m_issuesPanel->FitInside();
                m_issuesPanel->Layout();
                m_headerLabel->SetLabel(wxT("\U0001F3AF JIRA Issues (0)"));
            });
            return;
        }
        
        m_loading = true;
        
        // Update UI to show loading
        wxTheApp->CallAfter([this]() {
            m_statusLabel->SetLabel(wxT("\u23F3 Loading..."));
            m_statusLabel->SetForegroundColour(wxColour(52, 152, 219)); // Blue
        });
        
        // Copy client to capture for thread (it's safe to copy)
        Jira::Client client = m_jiraClient;
        
        // Fetch in background thread
        std::thread([this, client]() mutable {
            auto result = client.GetMyIssues(50);
            
            std::vector<JiraIssue> issues;
            wxString errorMsg;
            
            if (!result.success) {
                errorMsg = wxString::FromUTF8(result.error.c_str());
            } else {
                for (const auto& issue : result.data) {
                    issues.push_back(ConvertIssue(issue));
                }
            }
            
            // Update UI on main thread
            wxTheApp->CallAfter([this, issues, errorMsg]() {
                m_loading = false;
                m_issues = issues;
                
                // Clear and repopulate
                m_issuesSizer->Clear(true);
                
                if (!errorMsg.IsEmpty()) {
                    auto* errLabel = new wxStaticText(m_issuesPanel, wxID_ANY, errorMsg);
                    errLabel->SetForegroundColour(wxColour(231, 76, 60)); // Red
                    errLabel->Wrap(200);
                    m_issuesSizer->Add(errLabel, 0, wxALL, 10);
                    m_statusLabel->SetLabel(wxT("\u26A0 Error"));
                    m_statusLabel->SetForegroundColour(wxColour(231, 76, 60));
                } else if (m_issues.empty()) {
                    auto* emptyLabel = new wxStaticText(m_issuesPanel, wxID_ANY,
                        wxT("\U0001F389 No issues assigned to you!\n\nEnjoy your free time."));
                    emptyLabel->SetForegroundColour(wxColour(46, 204, 113));
                    emptyLabel->Wrap(200);
                    m_issuesSizer->Add(emptyLabel, 0, wxALL, 10);
                    auto& cfg = m_jiraClient.GetConfig();
                    m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s"), wxString::FromUTF8(cfg.user.c_str())));
                    m_statusLabel->SetForegroundColour(wxColour(46, 204, 113));
                } else {
                    for (const auto& issue : m_issues) {
                        auto* card = new JiraIssueCard(m_issuesPanel, issue);
                        card->SetThemeColors(m_bgColor, m_fgColor);
                        m_issuesSizer->Add(card, 0, wxEXPAND | wxBOTTOM, 5);
                    }
                    auto& cfg = m_jiraClient.GetConfig();
                    m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s"), wxString::FromUTF8(cfg.user.c_str())));
                    m_statusLabel->SetForegroundColour(wxColour(46, 204, 113));
                }
                
                m_issuesPanel->FitInside();
                m_issuesPanel->Layout();
                m_headerLabel->SetLabel(wxString::Format(wxT("\U0001F3AF JIRA Issues (%zu)"), m_issues.size()));
            });
        }).detach();
    }
    
    void OnRefresh(wxCommandEvent&) {
        RefreshIssues();
    }
    
    void OnShowMyIssues(wxCommandEvent&) {
        m_book->SetSelection(0);
    }
    
    void OnShowCreate(wxCommandEvent&) {
        ShowCreateForm();
    }
    
    void OnCreateIssue(wxCommandEvent&) {
        wxString summary = m_summaryText->GetValue().Trim();
        if (summary.IsEmpty()) {
            wxMessageBox("Please enter a summary for the issue.", "Missing Summary", 
                        wxOK | wxICON_WARNING, m_panel);
            m_summaryText->SetFocus();
            return;
        }
        
        if (!m_jiraClient.IsConfigured()) {
            wxMessageBox("Please configure JIRA settings first.", "Configuration Required",
                        wxOK | wxICON_WARNING, m_panel);
            return;
        }
        
        // Get selected values
        wxString project = m_projectChoice->GetStringSelection();
        wxString type = m_typeChoice->GetStringSelection();
        wxString priority = m_priorityChoice->GetStringSelection();
        wxString description = m_descText->GetValue();
        
        // Remove emoji prefixes for API call
        type = type.AfterFirst(' ');
        priority = priority.AfterFirst(' ');
        
        // Map type names to JIRA issue type names
        std::string issueType = "Task";
        if (type.Contains("Bug")) issueType = "Bug";
        else if (type.Contains("Story")) issueType = "Story";
        else if (type.Contains("Sub")) issueType = "Sub-task";
        
        // Map priority names to JIRA priority names
        std::string issuePriority = "Medium";
        if (priority.Contains("Highest")) issuePriority = "Highest";
        else if (priority.Contains("High")) issuePriority = "High";
        else if (priority.Contains("Low")) issuePriority = "Low";
        else if (priority.Contains("Lowest")) issuePriority = "Lowest";
        
        // Disable button while creating
        m_submitBtn->Disable();
        m_submitBtn->SetLabel("Creating...");
        
        // Copy values for thread
        std::string projectStr = project.ToStdString();
        std::string summaryStr = summary.ToStdString();
        std::string descStr = description.ToStdString();
        Jira::Client client = m_jiraClient;
        
        // Create issue in background thread
        std::thread([this, client, projectStr, summaryStr, issueType, descStr, issuePriority]() mutable {
            auto result = client.CreateIssue(projectStr, summaryStr, issueType, descStr, issuePriority);
            
            wxString newKey;
            wxString errorMsg;
            
            if (!result.success) {
                errorMsg = wxString::FromUTF8(result.error.c_str());
            } else {
                newKey = wxString::FromUTF8(result.data.c_str());
            }
            
            wxTheApp->CallAfter([this, newKey, errorMsg, client]() {
                m_submitBtn->Enable();
                m_submitBtn->SetLabel(wxT("\u2795 Create Issue"));
                
                if (!errorMsg.IsEmpty()) {
                    wxMessageBox(errorMsg, "Error", wxOK | wxICON_ERROR, m_panel);
                } else if (!newKey.IsEmpty()) {
                    // Clear form
                    m_summaryText->Clear();
                    m_descText->Clear();
                    
                    // Switch to issues view and refresh
                    m_book->SetSelection(0);
                    FetchIssuesFromApi();
                    
                    // Show success with link
                    wxString url = wxString::FromUTF8(client.GetConfig().apiUrl.c_str()) + "/browse/" + newKey;
                    int result = wxMessageBox(
                        wxString::Format("Issue %s created!\n\nClick OK to open in browser.", newKey),
                        "Success", wxOK | wxCANCEL | wxICON_INFORMATION, m_panel);
                    
                    if (result == wxOK) {
                        wxLaunchDefaultBrowser(url);
                    }
                }
            });
        }).detach();
    }
};

} // namespace BuiltinWidgets

#endif // JIRA_WIDGET_H
