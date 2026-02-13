#ifndef GITHUB_PROJECTS_WIDGET_H
#define GITHUB_PROJECTS_WIDGET_H

#include "widget.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "../github/github_projects_client.h"
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
 * GitHub issue structure for display (UI-specific with wxString).
 */
struct GitHubIssueItem {
    wxString key;           // e.g., "owner/repo#123"
    wxString summary;       // Issue title
    wxString status;        // Project status field value
    wxString priority;      // Priority (from labels or project field)
    wxString type;          // "Issue", "Pull Request", "Draft"
    wxString assignee;      // Assignee login
    wxString updated;       // Last updated timestamp
    wxString url;           // Web URL to the issue
    wxString projectItemId; // Internal project item node ID
};

/**
 * Custom panel for displaying a single GitHub Project item card.
 */
class GitHubIssueCard : public wxPanel {
public:
    GitHubIssueCard(wxWindow* parent, const GitHubIssueItem& issue)
        : wxPanel(parent, wxID_ANY)
        , m_issue(issue)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(wxSize(-1, 70));

        Bind(wxEVT_PAINT, &GitHubIssueCard::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &GitHubIssueCard::OnClick, this);
        Bind(wxEVT_ENTER_WINDOW, &GitHubIssueCard::OnMouseEnter, this);
        Bind(wxEVT_LEAVE_WINDOW, &GitHubIssueCard::OnMouseLeave, this);
    }

    void SetThemeColors(const wxColour& bg, const wxColour& fg) {
        m_bgColor = bg;
        m_fgColor = fg;
        Refresh();
    }

    const GitHubIssueItem& GetIssue() const { return m_issue; }

private:
    GitHubIssueItem m_issue;
    wxColour m_bgColor = wxColour(40, 40, 40);
    wxColour m_fgColor = wxColour(220, 220, 220);
    bool m_hovered = false;

    wxColour GetStatusColor() const {
        if (m_issue.status.Contains("Done") || m_issue.status.Contains("Closed") ||
            m_issue.status.Contains("Completed") || m_issue.status.Contains("Merged")) {
            return wxColour(46, 204, 113);  // Green
        } else if (m_issue.status.Contains("Progress") || m_issue.status.Contains("Review") ||
                   m_issue.status.Contains("Active")) {
            return wxColour(52, 152, 219);  // Blue
        } else if (m_issue.status.Contains("Block") || m_issue.status.Contains("Stale")) {
            return wxColour(231, 76, 60);   // Red
        } else {
            return wxColour(149, 165, 166); // Gray - Todo/Backlog
        }
    }

    wxColour GetTypeColor() const {
        if (m_issue.type.Contains("Pull")) {
            return wxColour(155, 89, 182);  // Purple - PR
        } else if (m_issue.type.Contains("Draft")) {
            return wxColour(149, 165, 166); // Gray - Draft
        } else {
            return wxColour(46, 204, 113);  // Green - Issue
        }
    }

    wxString GetTypeIcon() const {
        if (m_issue.type.Contains("Pull")) return wxT("\U0001F500"); // 🔀
        if (m_issue.type.Contains("Draft")) return wxT("\U0001F4DD"); // 📝
        return wxT("\U0001F4CB"); // 📋 Issue
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetClientSize();

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

        // Left status bar
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
        // Shorten key for display: show just "repo#123" if too long
        wxString displayKey = m_issue.key;
        if (displayKey.Length() > 25) {
            auto hashPos = displayKey.Find('#');
            if (hashPos != wxNOT_FOUND) {
                auto slashPos = displayKey.rfind('/', hashPos);
                if (slashPos != wxString::npos) {
                    displayKey = displayKey.Mid(slashPos + 1);
                }
            }
        }
        dc.DrawText(displayKey, x, y);

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
        if (!m_issue.status.IsEmpty()) {
            wxFont statusFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            dc.SetFont(statusFont);
            wxSize statusSize = dc.GetTextExtent(m_issue.status);

            dc.SetBrush(wxBrush(statusColor));
            dc.DrawRoundedRectangle(x, y, statusSize.GetWidth() + 10, 16, 3);

            dc.SetTextForeground(*wxWHITE);
            dc.DrawText(m_issue.status, x + 5, y + 1);
        }

        // Assignee (right side, if present)
        if (!m_issue.assignee.IsEmpty()) {
            wxFont assignFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            dc.SetFont(assignFont);
            dc.SetTextForeground(wxColour(120, 120, 120));
            wxString assignText = wxT("@") + m_issue.assignee;
            wxSize assignSize = dc.GetTextExtent(assignText);
            dc.DrawText(assignText, size.GetWidth() - assignSize.GetWidth() - 10, y + 1);
        }
    }

    void OnClick(wxMouseEvent&) {
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
 * GitHub Projects widget for tracking project items.
 * Displays issues/PRs/drafts with status indicators.
 */
class GitHubProjectsWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.githubProjects";
        info.name = "GitHub Projects";
        info.description = "View and manage GitHub Project items";
        info.location = WidgetLocation::Sidebar;
        info.category = WidgetCategories::Productivity();
        info.priority = 59;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        m_context = &context;
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        // Header
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
#ifdef __WXMSW__
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("[GH] GitHub Projects"));
#else
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("\U0001F4CA GitHub Projects")); // 📊
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
        m_refreshBtn = new wxButton(m_panel, wxID_ANY, wxT("\U0001F504"), wxDefaultPosition, wxSize(28, 24));
#endif
        m_refreshBtn->SetToolTip("Refresh items");
        headerSizer->Add(m_refreshBtn, 0, wxLEFT, 5);

        mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);

        // Connection status
        m_statusLabel = new wxStaticText(m_panel, wxID_ANY, "");
        wxFont statusFont = m_statusLabel->GetFont();
        statusFont.SetPointSize(8);
        m_statusLabel->SetFont(statusFont);
        mainSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

        // Tab buttons
        wxBoxSizer* tabSizer = new wxBoxSizer(wxHORIZONTAL);
        m_myItemsBtn = new wxButton(m_panel, wxID_ANY, "My Items", wxDefaultPosition, wxSize(-1, 26));
        m_allItemsBtn = new wxButton(m_panel, wxID_ANY, "All", wxDefaultPosition, wxSize(-1, 26));
        m_createBtn = new wxButton(m_panel, wxID_ANY, "+ New", wxDefaultPosition, wxSize(-1, 26));

        tabSizer->Add(m_myItemsBtn, 1, wxRIGHT, 2);
        tabSizer->Add(m_allItemsBtn, 1, wxRIGHT, 2);
        tabSizer->Add(m_createBtn, 0);
        mainSizer->Add(tabSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);

        mainSizer->AddSpacer(5);

        // Notebook for list vs create
        m_book = new wxSimplebook(m_panel);

        // === Items List Page ===
        m_itemsPanel = new wxScrolledWindow(m_book, wxID_ANY);
        m_itemsPanel->SetScrollRate(0, 10);
        m_itemsSizer = new wxBoxSizer(wxVERTICAL);
        m_itemsPanel->SetSizer(m_itemsSizer);
        m_book->AddPage(m_itemsPanel, "Items");

        // === Create Draft Page ===
        wxPanel* createPanel = new wxPanel(m_book);
        wxBoxSizer* createSizer = new wxBoxSizer(wxVERTICAL);

        // Title
        wxStaticText* titleLabel = new wxStaticText(createPanel, wxID_ANY, "Title:");
        m_titleText = new wxTextCtrl(createPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
        m_titleText->SetHint("Enter draft issue title...");
        createSizer->Add(titleLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_titleText, 0, wxEXPAND | wxALL, 5);

        // Body
        wxStaticText* bodyLabel = new wxStaticText(createPanel, wxID_ANY, "Body:");
        m_bodyText = new wxTextCtrl(createPanel, wxID_ANY, "", wxDefaultPosition,
                                     wxSize(-1, 80), wxTE_MULTILINE);
        m_bodyText->SetHint("Enter description (optional)...");
        createSizer->Add(bodyLabel, 0, wxLEFT | wxTOP, 5);
        createSizer->Add(m_bodyText, 1, wxEXPAND | wxALL, 5);

        // Create button
        wxBoxSizer* createBtnSizer = new wxBoxSizer(wxHORIZONTAL);
        createBtnSizer->AddStretchSpacer();
        m_submitBtn = new wxButton(createPanel, wxID_ANY, wxT("\u2795 Create Draft")); // ➕
        createBtnSizer->Add(m_submitBtn, 0, wxALL, 5);
        createSizer->Add(createBtnSizer, 0, wxEXPAND);

        createPanel->SetSizer(createSizer);
        m_book->AddPage(createPanel, "Create");

        mainSizer->Add(m_book, 1, wxEXPAND | wxALL, 5);

        // Legend footer
        m_legendLabel = new wxStaticText(m_panel, wxID_ANY,
            wxT("\U0001F7E2 Done  \U0001F535 In Progress  \u26AA Todo  \U0001F534 Blocked"));
        wxFont legendFont = m_legendLabel->GetFont();
        legendFont.SetPointSize(7);
        m_legendLabel->SetFont(legendFont);
        mainSizer->Add(m_legendLabel, 0, wxALL | wxALIGN_CENTER, 5);

        m_panel->SetSizer(mainSizer);

        // Bind events
        m_refreshBtn->Bind(wxEVT_BUTTON, &GitHubProjectsWidget::OnRefresh, this);
        m_myItemsBtn->Bind(wxEVT_BUTTON, &GitHubProjectsWidget::OnShowMyItems, this);
        m_allItemsBtn->Bind(wxEVT_BUTTON, &GitHubProjectsWidget::OnShowAllItems, this);
        m_createBtn->Bind(wxEVT_BUTTON, &GitHubProjectsWidget::OnShowCreate, this);
        m_submitBtn->Bind(wxEVT_BUTTON, &GitHubProjectsWidget::OnCreateDraft, this);

        LoadConfig();
        FetchItemsFromApi(false); // fetch all items first

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
        m_itemsPanel->SetBackgroundColour(m_bgColor);
        if (m_legendLabel) {
            m_legendLabel->SetForegroundColour(wxColour(150, 150, 150));
        }

        for (wxWindow* child : m_itemsPanel->GetChildren()) {
            if (auto* card = dynamic_cast<GitHubIssueCard*>(child)) {
                card->SetThemeColors(m_bgColor, m_fgColor);
            }
        }

        m_panel->Refresh();
    }

    std::vector<wxString> GetCommands() const override {
        return {
            "github.projects.refresh",
            "github.projects.show",
            "github.projects.hide",
            "github.projects.toggle",
            "github.projects.create",
            "github.projects.configure"
        };
    }

    void RegisterCommands(WidgetContext& context) override {
        auto& registry = CommandRegistry::Instance();
        m_context = &context;

        auto makeCmd = [](const wxString& id, const wxString& title,
                         const wxString& desc, Command::ExecuteFunc exec,
                         Command::EnabledFunc enabled = nullptr) {
            auto cmd = std::make_shared<Command>(id, title, "GitHub Projects");
            cmd->SetDescription(desc);
            cmd->SetExecuteHandler(std::move(exec));
            if (enabled) cmd->SetEnabledHandler(std::move(enabled));
            return cmd;
        };

        GitHubProjectsWidget* self = this;

        registry.Register(makeCmd(
            "github.projects.toggle", "Toggle GitHub Projects Widget",
            "Show or hide the GitHub Projects widget",
            [self](CommandContext& ctx) { self->ToggleVisibility(ctx); }
        ));

        registry.Register(makeCmd(
            "github.projects.show", "Show GitHub Projects Widget",
            "Show the GitHub Projects widget in the sidebar",
            [self](CommandContext& ctx) { self->SetVisible(true, ctx); }
        ));

        registry.Register(makeCmd(
            "github.projects.hide", "Hide GitHub Projects Widget",
            "Hide the GitHub Projects widget",
            [self](CommandContext& ctx) { self->SetVisible(false, ctx); },
            [self](const CommandContext& ctx) { return self->IsVisible(); }
        ));

        registry.Register(makeCmd(
            "github.projects.refresh", "Refresh GitHub Projects",
            "Refresh the list of project items",
            [self](CommandContext& ctx) { self->RefreshItems(); }
        ));

        registry.Register(makeCmd(
            "github.projects.create", "Create Draft Issue",
            "Open the draft issue creation form",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->ShowCreateForm();
            }
        ));

        registry.Register(makeCmd(
            "github.projects.configure", "Configure GitHub Projects",
            "Open GitHub Projects settings in config file",
            [self](CommandContext& ctx) { self->OpenConfiguration(); }
        ));
    }

    wxWindow* GetWindow() { return m_panel; }

    bool IsVisible() const {
        return m_panel && m_panel->IsShown();
    }

    void SetVisible(bool visible, CommandContext& ctx) {
        auto* frame = ctx.Get<MainFrame>("mainFrame");
        if (frame) {
            frame->ShowSidebarWidget("core.githubProjects", visible);
        }
    }

    void ToggleVisibility(CommandContext& ctx) {
        SetVisible(!IsVisible(), ctx);
    }

    void RefreshItems() {
        if (!m_loading) {
            FetchItemsFromApi(m_showMyItems);
        }
    }

    void ShowCreateForm() {
        if (m_book) {
            m_book->SetSelection(1);
            m_titleText->SetFocus();
        }
    }

    void OpenConfiguration() {
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
    wxButton* m_myItemsBtn = nullptr;
    wxButton* m_allItemsBtn = nullptr;
    wxButton* m_createBtn = nullptr;
    wxSimplebook* m_book = nullptr;
    wxScrolledWindow* m_itemsPanel = nullptr;
    wxBoxSizer* m_itemsSizer = nullptr;
    wxTextCtrl* m_titleText = nullptr;
    wxTextCtrl* m_bodyText = nullptr;
    wxButton* m_submitBtn = nullptr;
    WidgetContext* m_context = nullptr;

    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);

    std::vector<GitHubIssueItem> m_items;
    std::atomic<bool> m_loading{false};
    bool m_showMyItems = false;

    GitHub::ProjectsClient m_ghClient;

    GitHubIssueItem ConvertIssue(const GitHub::Issue& issue) {
        GitHubIssueItem item;
        item.key = wxString::FromUTF8(issue.key.c_str());
        item.summary = wxString::FromUTF8(issue.summary.c_str());
        item.status = wxString::FromUTF8(issue.status.c_str());
        item.priority = wxString::FromUTF8(issue.priority.c_str());
        item.type = wxString::FromUTF8(issue.type.c_str());
        item.assignee = wxString::FromUTF8(issue.assignee.c_str());
        item.updated = FormatRelativeTime(wxString::FromUTF8(issue.updated.c_str()));
        item.url = wxString::FromUTF8(issue.url.c_str());
        item.projectItemId = wxString::FromUTF8(issue.projectItemId.c_str());
        return item;
    }

    wxString FormatRelativeTime(const wxString& isoTime) {
        // Parse ISO 8601 timestamp (GitHub format: 2026-01-19T10:30:00Z)
        wxDateTime dt;
        if (!dt.ParseISOCombined(isoTime.BeforeFirst('Z').BeforeFirst('.'))) {
            return isoTime.Left(10);
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
        auto config = GitHub::ClientConfig::LoadFromConfig();
        m_ghClient.SetConfig(config);

        if (!m_ghClient.IsConfigured()) {
            m_statusLabel->SetLabel(wxT("\u26A0 Configure github.token, github.owner, github.projectNumber"));
            m_statusLabel->SetForegroundColour(wxColour(241, 196, 15));
        } else {
            m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s (project #%d)"),
                wxString::FromUTF8(config.owner.c_str()), config.projectNumber));
            m_statusLabel->SetForegroundColour(wxColour(46, 204, 113));
        }
    }

    void FetchItemsFromApi(bool myItemsOnly) {
        if (!m_ghClient.IsConfigured()) {
            wxTheApp->CallAfter([this]() {
                m_itemsSizer->Clear(true);
                m_items.clear();

                auto* msgLabel = new wxStaticText(m_itemsPanel, wxID_ANY,
                    "Please configure GitHub Projects:\n\n"
                    "1. github.token (PAT with project scope)\n"
                    "2. github.owner (org or user)\n"
                    "3. github.projectNumber\n\n"
                    "Generate a token at:\n"
                    "https://github.com/settings/tokens");
                msgLabel->SetForegroundColour(wxColour(180, 180, 180));
                msgLabel->Wrap(200);
                m_itemsSizer->Add(msgLabel, 0, wxALL, 10);

                m_itemsPanel->FitInside();
                m_itemsPanel->Layout();
                m_headerLabel->SetLabel(wxT("\U0001F4CA GitHub Projects (0)"));
            });
            return;
        }

        m_loading = true;
        m_showMyItems = myItemsOnly;

        wxTheApp->CallAfter([this]() {
            m_statusLabel->SetLabel(wxT("\u23F3 Loading..."));
            m_statusLabel->SetForegroundColour(wxColour(52, 152, 219));
        });

        GitHub::ProjectsClient client = m_ghClient;
        bool fetchMine = myItemsOnly;

        std::thread([this, client, fetchMine]() mutable {
            GitHub::Result<std::vector<GitHub::Issue>> result;
            if (fetchMine) {
                result = client.GetMyItems(50);
            } else {
                result = client.ListItems(50);
            }

            std::vector<GitHubIssueItem> items;
            wxString errorMsg;

            if (!result.success) {
                errorMsg = wxString::FromUTF8(result.error.c_str());
            } else {
                for (const auto& issue : result.data) {
                    items.push_back(ConvertIssue(issue));
                }
            }

            wxTheApp->CallAfter([this, items, errorMsg]() {
                m_loading = false;
                m_items = items;

                m_itemsSizer->Clear(true);

                if (!errorMsg.IsEmpty()) {
                    auto* errLabel = new wxStaticText(m_itemsPanel, wxID_ANY, errorMsg);
                    errLabel->SetForegroundColour(wxColour(231, 76, 60));
                    errLabel->Wrap(200);
                    m_itemsSizer->Add(errLabel, 0, wxALL, 10);
                    m_statusLabel->SetLabel(wxT("\u26A0 Error"));
                    m_statusLabel->SetForegroundColour(wxColour(231, 76, 60));
                } else if (m_items.empty()) {
                    auto* emptyLabel = new wxStaticText(m_itemsPanel, wxID_ANY,
                        wxT("\U0001F389 No items found!\n\nThe project board is empty."));
                    emptyLabel->SetForegroundColour(wxColour(46, 204, 113));
                    emptyLabel->Wrap(200);
                    m_itemsSizer->Add(emptyLabel, 0, wxALL, 10);
                    auto& cfg = m_ghClient.GetConfig();
                    m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s (project #%d)"),
                        wxString::FromUTF8(cfg.owner.c_str()), cfg.projectNumber));
                    m_statusLabel->SetForegroundColour(wxColour(46, 204, 113));
                } else {
                    for (const auto& item : m_items) {
                        auto* card = new GitHubIssueCard(m_itemsPanel, item);
                        card->SetThemeColors(m_bgColor, m_fgColor);
                        m_itemsSizer->Add(card, 0, wxEXPAND | wxBOTTOM, 5);
                    }
                    auto& cfg = m_ghClient.GetConfig();
                    m_statusLabel->SetLabel(wxString::Format(wxT("\u2713 %s (project #%d)"),
                        wxString::FromUTF8(cfg.owner.c_str()), cfg.projectNumber));
                    m_statusLabel->SetForegroundColour(wxColour(46, 204, 113));
                }

                m_itemsPanel->FitInside();
                m_itemsPanel->Layout();
                m_headerLabel->SetLabel(wxString::Format(wxT("\U0001F4CA GitHub Projects (%zu)"), m_items.size()));
            });
        }).detach();
    }

    void OnRefresh(wxCommandEvent&) { RefreshItems(); }
    void OnShowMyItems(wxCommandEvent&) { m_book->SetSelection(0); FetchItemsFromApi(true); }
    void OnShowAllItems(wxCommandEvent&) { m_book->SetSelection(0); FetchItemsFromApi(false); }
    void OnShowCreate(wxCommandEvent&) { ShowCreateForm(); }

    void OnCreateDraft(wxCommandEvent&) {
        wxString title = m_titleText->GetValue().Trim();
        if (title.IsEmpty()) {
            wxMessageBox("Please enter a title for the draft.", "Missing Title",
                        wxOK | wxICON_WARNING, m_panel);
            m_titleText->SetFocus();
            return;
        }

        if (!m_ghClient.IsConfigured()) {
            wxMessageBox("Please configure GitHub Projects settings first.", "Configuration Required",
                        wxOK | wxICON_WARNING, m_panel);
            return;
        }

        m_submitBtn->Disable();
        m_submitBtn->SetLabel("Creating...");

        std::string titleStr = title.ToStdString();
        std::string bodyStr = m_bodyText->GetValue().ToStdString();
        GitHub::ProjectsClient client = m_ghClient;

        std::thread([this, client, titleStr, bodyStr]() mutable {
            auto result = client.CreateDraftIssue(titleStr, bodyStr);

            wxString newItemId;
            wxString errorMsg;

            if (!result.success) {
                errorMsg = wxString::FromUTF8(result.error.c_str());
            } else {
                newItemId = wxString::FromUTF8(result.data.c_str());
            }

            wxTheApp->CallAfter([this, newItemId, errorMsg]() {
                m_submitBtn->Enable();
                m_submitBtn->SetLabel(wxT("\u2795 Create Draft"));

                if (!errorMsg.IsEmpty()) {
                    wxMessageBox(errorMsg, "Error", wxOK | wxICON_ERROR, m_panel);
                } else if (!newItemId.IsEmpty()) {
                    m_titleText->Clear();
                    m_bodyText->Clear();
                    m_book->SetSelection(0);
                    FetchItemsFromApi(m_showMyItems);

                    wxMessageBox(
                        wxString::Format("Draft issue created!\nItem ID: %s", newItemId),
                        "Success", wxOK | wxICON_INFORMATION, m_panel);
                }
            });
        }).detach();
    }
};

} // namespace BuiltinWidgets

#endif // GITHUB_PROJECTS_WIDGET_H
