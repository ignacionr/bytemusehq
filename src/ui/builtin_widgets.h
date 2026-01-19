#ifndef BUILTIN_WIDGETS_H
#define BUILTIN_WIDGETS_H

#include "widget.h"
#include "editor.h"
#include "terminal.h"
#include "../theme/theme.h"
#include "../commands/command.h"
#include "../commands/command_registry.h"
#include <wx/treectrl.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/dcbuffer.h>
#include <wx/timer.h>
#include <cmath>

// Forward declaration
class MainFrame;

/**
 * Built-in widget implementations for ByteMuseHQ.
 * 
 * These widgets provide the core UI components:
 * - FileTreeWidget: File browser sidebar
 * - EditorWidget: Code editor (center)
 * - TerminalWidget: Integrated terminal (bottom panel)
 * - TimerWidget: Pomodoro focus timer (sidebar)
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
// TimerWidget - Pomodoro-style timer with visual progress
// ============================================================================

/**
 * Custom panel for drawing the timer visualization.
 */
class TimerPanel : public wxPanel {
public:
    TimerPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
        , m_totalSeconds(25 * 60)
        , m_remainingSeconds(25 * 60)
        , m_isRunning(false)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &TimerPanel::OnPaint, this);
        
        m_timer.Bind(wxEVT_TIMER, &TimerPanel::OnTimer, this);
    }
    
    void SetTotalTime(int seconds) {
        m_totalSeconds = seconds;
        if (!m_isRunning) {
            m_remainingSeconds = seconds;
        }
        Refresh();
    }
    
    void SetRemainingTime(int seconds) {
        m_remainingSeconds = seconds;
        Refresh();
    }
    
    int GetRemainingSeconds() const { return m_remainingSeconds; }
    int GetTotalSeconds() const { return m_totalSeconds; }
    bool IsRunning() const { return m_isRunning; }
    
    void Start() {
        if (!m_isRunning && m_remainingSeconds > 0) {
            m_isRunning = true;
            m_timer.Start(1000);
            Refresh();
        }
    }
    
    void Pause() {
        m_isRunning = false;
        m_timer.Stop();
        Refresh();
    }
    
    void Reset() {
        m_isRunning = false;
        m_timer.Stop();
        m_remainingSeconds = m_totalSeconds;
        Refresh();
    }
    
    void SetThemeColors(const wxColour& bg, const wxColour& fg) {
        m_backgroundColor = bg;
        m_foregroundColor = fg;
        Refresh();
    }
    
private:
    wxTimer m_timer;
    int m_totalSeconds;
    int m_remainingSeconds;
    bool m_isRunning;
    wxColour m_backgroundColor = wxColour(30, 30, 30);
    wxColour m_foregroundColor = wxColour(220, 220, 220);
    
    void OnTimer(wxTimerEvent&) {
        if (m_remainingSeconds > 0) {
            m_remainingSeconds--;
            Refresh();
            
            if (m_remainingSeconds == 0) {
                m_isRunning = false;
                m_timer.Stop();
                // Flash effect could be added here
            }
        }
    }
    
    wxColour InterpolateColor(const wxColour& c1, const wxColour& c2, double t) {
        return wxColour(
            (unsigned char)(c1.Red() + t * (c2.Red() - c1.Red())),
            (unsigned char)(c1.Green() + t * (c2.Green() - c1.Green())),
            (unsigned char)(c1.Blue() + t * (c2.Blue() - c1.Blue()))
        );
    }
    
    wxColour GetProgressColor() {
        double progress = (double)m_remainingSeconds / m_totalSeconds;
        
        // Gradient from green (full) -> yellow (half) -> orange -> red (empty)
        if (progress > 0.66) {
            // Green to cyan
            double t = (progress - 0.66) / 0.34;
            return InterpolateColor(wxColour(0, 230, 180), wxColour(100, 255, 150), t);
        } else if (progress > 0.33) {
            // Yellow to green
            double t = (progress - 0.33) / 0.33;
            return InterpolateColor(wxColour(255, 180, 50), wxColour(0, 230, 180), t);
        } else if (progress > 0.1) {
            // Orange to yellow
            double t = (progress - 0.1) / 0.23;
            return InterpolateColor(wxColour(255, 100, 50), wxColour(255, 180, 50), t);
        } else {
            // Red to orange (urgent!)
            double t = progress / 0.1;
            return InterpolateColor(wxColour(255, 50, 80), wxColour(255, 100, 50), t);
        }
    }
    
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetClientSize();
        
        // Clear background
        dc.SetBackground(wxBrush(m_backgroundColor));
        dc.Clear();
        
        int centerX = size.GetWidth() / 2;
        int centerY = size.GetHeight() / 2;
        int radius = std::min(centerX, centerY) - 20;
        
        if (radius < 30) return; // Too small to draw
        
        // Draw outer glow ring (subtle)
        wxColour progressColor = GetProgressColor();
        wxColour glowColor(progressColor.Red(), progressColor.Green(), progressColor.Blue(), 30);
        dc.SetPen(wxPen(glowColor, 12));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(centerX, centerY, radius + 5);
        
        // Draw background circle (track)
        wxColour trackColor = m_backgroundColor.IsOk() 
            ? wxColour(
                std::min(255, m_backgroundColor.Red() + 25),
                std::min(255, m_backgroundColor.Green() + 25),
                std::min(255, m_backgroundColor.Blue() + 25)
              )
            : wxColour(55, 55, 55);
        dc.SetPen(wxPen(trackColor, 8));
        dc.DrawCircle(centerX, centerY, radius);
        
        // Draw progress arc
        double progress = (double)m_remainingSeconds / m_totalSeconds;
        if (progress > 0) {
            dc.SetPen(wxPen(progressColor, 8));
            
            // Draw arc segments for smooth appearance
            int segments = (int)(progress * 100);
            double startAngle = M_PI / 2; // Start at top (90 degrees)
            double totalAngle = 2 * M_PI * progress;
            
            for (int i = 0; i < segments; i++) {
                double angle1 = startAngle - (totalAngle * i / segments);
                double angle2 = startAngle - (totalAngle * (i + 1) / segments);
                
                int x1 = centerX + (int)(radius * cos(angle1));
                int y1 = centerY - (int)(radius * sin(angle1));
                int x2 = centerX + (int)(radius * cos(angle2));
                int y2 = centerY - (int)(radius * sin(angle2));
                
                dc.DrawLine(x1, y1, x2, y2);
            }
        }
        
        // Draw center decoration
        dc.SetPen(*wxTRANSPARENT_PEN);
        wxColour innerColor = m_isRunning ? progressColor : m_foregroundColor;
        innerColor = wxColour(innerColor.Red(), innerColor.Green(), innerColor.Blue(), 40);
        dc.SetBrush(wxBrush(wxColour(
            m_backgroundColor.Red() + (innerColor.Red() - m_backgroundColor.Red()) / 8,
            m_backgroundColor.Green() + (innerColor.Green() - m_backgroundColor.Green()) / 8,
            m_backgroundColor.Blue() + (innerColor.Blue() - m_backgroundColor.Blue()) / 8
        )));
        dc.DrawCircle(centerX, centerY, radius - 15);
        
        // Draw time text
        int minutes = m_remainingSeconds / 60;
        int seconds = m_remainingSeconds % 60;
        wxString timeStr = wxString::Format("%02d:%02d", minutes, seconds);
        
        // Large time display
        wxFont timeFont(radius / 3, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(timeFont);
        dc.SetTextForeground(progressColor);
        
        wxSize textSize = dc.GetTextExtent(timeStr);
        dc.DrawText(timeStr, centerX - textSize.GetWidth() / 2, centerY - textSize.GetHeight() / 2 - 5);
        
        // Status text below time
        wxString statusStr = m_isRunning ? "FOCUS" : (m_remainingSeconds == m_totalSeconds ? "READY" : "PAUSED");
        wxFont statusFont(radius / 8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        dc.SetFont(statusFont);
        dc.SetTextForeground(wxColour(
            m_foregroundColor.Red(),
            m_foregroundColor.Green(),
            m_foregroundColor.Blue(),
            180
        ));
        
        wxSize statusSize = dc.GetTextExtent(statusStr);
        dc.DrawText(statusStr, centerX - statusSize.GetWidth() / 2, centerY + textSize.GetHeight() / 2);
        
        // Draw decorative dots at quarter marks
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(trackColor));
        for (int i = 0; i < 4; i++) {
            double angle = M_PI / 2 - i * M_PI / 2;
            int dotX = centerX + (int)((radius + 15) * cos(angle));
            int dotY = centerY - (int)((radius + 15) * sin(angle));
            dc.DrawCircle(dotX, dotY, 3);
        }
    }
};

/**
 * Timer widget for Pomodoro-style focus sessions.
 * Default: 25-minute timer with colorful visual progress display.
 */
class TimerWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.timer";
        info.name = "Focus Timer";
        info.description = "Pomodoro-style focus timer with visual progress";
        info.location = WidgetLocation::Sidebar;
        info.priority = 50;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        m_context = &context;
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // Header
        wxStaticText* headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("\u23F1 Focus Timer"));
        wxFont headerFont = headerLabel->GetFont();
        headerFont.SetWeight(wxFONTWEIGHT_BOLD);
        headerLabel->SetFont(headerFont);
        mainSizer->Add(headerLabel, 0, wxALL | wxALIGN_CENTER, 5);
        
        // Timer visualization
        m_timerPanel = new TimerPanel(m_panel);
        m_timerPanel->SetMinSize(wxSize(180, 180));
        mainSizer->Add(m_timerPanel, 1, wxEXPAND | wxALL, 5);
        
        // Control buttons
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        
        m_startPauseBtn = new wxButton(m_panel, wxID_ANY, "Start");
        m_resetBtn = new wxButton(m_panel, wxID_ANY, "Reset");
        
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(m_startPauseBtn, 0, wxRIGHT, 5);
        buttonSizer->Add(m_resetBtn, 0);
        buttonSizer->AddStretchSpacer();
        
        mainSizer->Add(buttonSizer, 0, wxEXPAND | wxBOTTOM, 5);
        
        // Preset buttons
        wxBoxSizer* presetSizer = new wxBoxSizer(wxHORIZONTAL);
        
        wxButton* btn5 = new wxButton(m_panel, wxID_ANY, "5m", wxDefaultPosition, wxSize(40, -1));
        wxButton* btn15 = new wxButton(m_panel, wxID_ANY, "15m", wxDefaultPosition, wxSize(40, -1));
        wxButton* btn25 = new wxButton(m_panel, wxID_ANY, "25m", wxDefaultPosition, wxSize(40, -1));
        wxButton* btn45 = new wxButton(m_panel, wxID_ANY, "45m", wxDefaultPosition, wxSize(40, -1));
        
        presetSizer->AddStretchSpacer();
        presetSizer->Add(btn5, 0, wxRIGHT, 2);
        presetSizer->Add(btn15, 0, wxRIGHT, 2);
        presetSizer->Add(btn25, 0, wxRIGHT, 2);
        presetSizer->Add(btn45, 0);
        presetSizer->AddStretchSpacer();
        
        mainSizer->Add(presetSizer, 0, wxEXPAND | wxBOTTOM, 10);
        
        m_panel->SetSizer(mainSizer);
        
        // Bind events
        m_startPauseBtn->Bind(wxEVT_BUTTON, &TimerWidget::OnStartPause, this);
        m_resetBtn->Bind(wxEVT_BUTTON, &TimerWidget::OnReset, this);
        btn5->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPreset(5); });
        btn15->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPreset(15); });
        btn25->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPreset(25); });
        btn45->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPreset(45); });
        
        // Update button state periodically
        m_updateTimer.Bind(wxEVT_TIMER, &TimerWidget::OnUpdateUI, this);
        m_updateTimer.Start(500);
        
        return m_panel;
    }

    void OnThemeChanged(wxWindow* window, WidgetContext& context) override {
        auto theme = ThemeManager::Instance().GetCurrentTheme();
        if (!theme || !m_panel) return;
        
        m_panel->SetBackgroundColour(theme->ui.sidebarBackground);
        
        // Update header text color
        for (wxWindow* child : m_panel->GetChildren()) {
            if (auto* staticText = dynamic_cast<wxStaticText*>(child)) {
                staticText->SetForegroundColour(theme->ui.sidebarForeground);
            }
        }
        
        if (m_timerPanel) {
            m_timerPanel->SetThemeColors(theme->ui.sidebarBackground, theme->ui.sidebarForeground);
        }
        
        m_panel->Refresh();
    }

    std::vector<wxString> GetCommands() const override {
        return {
            "timer.toggle",
            "timer.start",
            "timer.pause", 
            "timer.reset",
            "timer.set5",
            "timer.set15",
            "timer.set25",
            "timer.set45"
        };
    }

    void RegisterCommands(WidgetContext& context) override {
        auto& registry = CommandRegistry::Instance();
        m_context = &context;
        
        // Helper to create timer commands
        auto makeCmd = [](const wxString& id, const wxString& title, 
                         const wxString& desc, Command::ExecuteFunc exec,
                         Command::EnabledFunc enabled = nullptr) {
            auto cmd = std::make_shared<Command>(id, title, "Timer");
            cmd->SetDescription(desc);
            cmd->SetExecuteHandler(std::move(exec));
            if (enabled) cmd->SetEnabledHandler(std::move(enabled));
            return cmd;
        };
        
        // Capture this for lambdas
        TimerWidget* self = this;
        
        registry.Register(makeCmd(
            "timer.toggle", "Toggle Focus Timer", 
            "Show or hide the focus timer",
            [self](CommandContext& ctx) {
                self->ToggleVisibility(ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "timer.show", "Show Focus Timer",
            "Show the focus timer in the sidebar",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "timer.hide", "Hide Focus Timer",
            "Hide the focus timer",
            [self](CommandContext& ctx) {
                self->SetVisible(false, ctx);
            },
            [self](const CommandContext& ctx) {
                return self->IsVisible();
            }
        ));
        
        registry.Register(makeCmd(
            "timer.start", "Start Timer",
            "Start the focus timer",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->StartTimer();
            }
        ));
        
        registry.Register(makeCmd(
            "timer.pause", "Pause Timer",
            "Pause the focus timer",
            [self](CommandContext& ctx) {
                self->PauseTimer();
            },
            [self](const CommandContext& ctx) {
                return self->m_timerPanel && self->m_timerPanel->IsRunning();
            }
        ));
        
        registry.Register(makeCmd(
            "timer.reset", "Reset Timer",
            "Reset the focus timer to its initial duration",
            [self](CommandContext& ctx) {
                self->ResetTimer();
            }
        ));
        
        registry.Register(makeCmd(
            "timer.set5", "Set Timer: 5 minutes",
            "Set timer to 5 minutes (short break)",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->SetPreset(5);
            }
        ));
        
        registry.Register(makeCmd(
            "timer.set15", "Set Timer: 15 minutes",
            "Set timer to 15 minutes (long break)",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->SetPreset(15);
            }
        ));
        
        registry.Register(makeCmd(
            "timer.set25", "Set Timer: 25 minutes",
            "Set timer to 25 minutes (Pomodoro)",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->SetPreset(25);
            }
        ));
        
        registry.Register(makeCmd(
            "timer.set45", "Set Timer: 45 minutes",
            "Set timer to 45 minutes (deep work)",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
                self->SetPreset(45);
            }
        ));
    }

    // Public API for command access
    TimerPanel* GetTimerPanel() { return m_timerPanel; }
    wxWindow* GetWindow() { return m_panel; }
    
    void StartTimer() {
        if (m_timerPanel) m_timerPanel->Start();
        UpdateButtonLabel();
    }
    
    void PauseTimer() {
        if (m_timerPanel) m_timerPanel->Pause();
        UpdateButtonLabel();
    }
    
    void ResetTimer() {
        if (m_timerPanel) m_timerPanel->Reset();
        UpdateButtonLabel();
    }
    
    bool IsVisible() const { 
        return m_panel && m_panel->IsShown(); 
    }
    
    void SetVisible(bool visible, CommandContext& ctx) {
        // Use the frame's generic sidebar widget visibility management
        auto* frame = ctx.Get<MainFrame>("mainFrame");
        if (frame) {
            frame->ShowSidebarWidget("core.timer", visible);
        }
    }
    
    void ToggleVisibility(CommandContext& ctx) {
        SetVisible(!IsVisible(), ctx);
    }

private:
    wxPanel* m_panel = nullptr;
    TimerPanel* m_timerPanel = nullptr;
    wxButton* m_startPauseBtn = nullptr;
    wxButton* m_resetBtn = nullptr;
    wxTimer m_updateTimer;
    WidgetContext* m_context = nullptr;
    
    void OnStartPause(wxCommandEvent&) {
        if (m_timerPanel->IsRunning()) {
            m_timerPanel->Pause();
        } else {
            m_timerPanel->Start();
        }
        UpdateButtonLabel();
    }
    
    void OnReset(wxCommandEvent&) {
        m_timerPanel->Reset();
        UpdateButtonLabel();
    }
    
    void SetPreset(int minutes) {
        if (m_timerPanel) {
            m_timerPanel->Pause();
            m_timerPanel->SetTotalTime(minutes * 60);
            m_timerPanel->Reset();
        }
        UpdateButtonLabel();
    }
    
    void OnUpdateUI(wxTimerEvent&) {
        UpdateButtonLabel();
    }
    
    void UpdateButtonLabel() {
        if (m_startPauseBtn && m_timerPanel) {
            m_startPauseBtn->SetLabel(m_timerPanel->IsRunning() ? "Pause" : "Start");
        }
    }
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
    
    registry.Register("core.timer", []() -> WidgetPtr {
        return std::make_shared<TimerWidget>();
    });
}

} // namespace BuiltinWidgets

#endif // BUILTIN_WIDGETS_H
