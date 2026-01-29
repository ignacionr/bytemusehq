#ifndef GEMINI_CHAT_WIDGET_H
#define GEMINI_CHAT_WIDGET_H

#include "widget.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "../ai/gemini_client.h"
#include <wx/dcbuffer.h>
#include <wx/timer.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/wrapsizer.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/tokenzr.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

// Forward declaration
class MainFrame;

namespace BuiltinWidgets {

/**
 * Custom panel for displaying a single chat message bubble.
 */
class ChatMessageBubble : public wxPanel {
public:
    ChatMessageBubble(wxWindow* parent, const wxString& text, bool isUser, bool isError = false)
        : wxPanel(parent, wxID_ANY)
        , m_text(text)
        , m_isUser(isUser)
        , m_isError(isError)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        
        // Calculate minimum height based on text
        wxClientDC dc(this);
        dc.SetFont(GetFont());
        int lineHeight = dc.GetCharHeight();
        int width = 280; // Approximate bubble width
        
        wxArrayInt widths;
        wxString wrapped = WrapText(text, width - 20, dc);
        int lines = 1;
        for (size_t i = 0; i < wrapped.Length(); ++i) {
            if (wrapped[i] == '\n') lines++;
        }
        
        int height = std::max(36, lines * lineHeight + 20);
        SetMinSize(wxSize(-1, height));
        
        Bind(wxEVT_PAINT, &ChatMessageBubble::OnPaint, this);
    }
    
    void SetThemeColors(const wxColour& bg, const wxColour& fg) {
        m_bgColor = bg;
        m_fgColor = fg;
        Refresh();
    }
    
    void SetText(const wxString& text) {
        m_text = text;
        
        // Recalculate height
        wxClientDC dc(this);
        dc.SetFont(GetFont());
        int lineHeight = dc.GetCharHeight();
        int width = 280;
        
        wxString wrapped = WrapText(m_text, width - 20, dc);
        int lines = 1;
        for (size_t i = 0; i < wrapped.Length(); ++i) {
            if (wrapped[i] == '\n') lines++;
        }
        
        int height = std::max(36, lines * lineHeight + 20);
        SetMinSize(wxSize(-1, height));
        
        GetParent()->Layout();
        Refresh();
    }
    
private:
    wxString m_text;
    bool m_isUser;
    bool m_isError;
    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);
    
    wxString WrapText(const wxString& text, int maxWidth, wxDC& dc) {
        wxString result;
        wxString line;
        wxString word;
        
        for (size_t i = 0; i <= text.Length(); ++i) {
            char c = (i < text.Length()) ? static_cast<char>(text[i].GetValue()) : ' ';
            
            if (c == ' ' || c == '\n' || i == text.Length()) {
                wxString testLine = line.IsEmpty() ? word : line + " " + word;
                wxSize extent = dc.GetTextExtent(testLine);
                
                if (extent.GetWidth() > maxWidth && !line.IsEmpty()) {
                    if (!result.IsEmpty()) result += "\n";
                    result += line;
                    line = word;
                } else {
                    line = testLine;
                }
                
                if (c == '\n') {
                    if (!result.IsEmpty()) result += "\n";
                    result += line;
                    line.Clear();
                }
                
                word.Clear();
            } else {
                word += c;
            }
        }
        
        if (!line.IsEmpty()) {
            if (!result.IsEmpty()) result += "\n";
            result += line;
        }
        
        return result;
    }
    
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetClientSize();
        
        // Clear background
        dc.SetBrush(wxBrush(m_bgColor));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());
        
        // Bubble colors
        wxColour bubbleBg;
        wxColour bubbleFg;
        
        if (m_isError) {
            bubbleBg = wxColour(120, 40, 40);
            bubbleFg = wxColour(255, 180, 180);
        } else if (m_isUser) {
            bubbleBg = wxColour(59, 130, 246); // Blue for user
            bubbleFg = wxColour(255, 255, 255);
        } else {
            // Model response - adapt to theme
            bubbleBg = wxColour(
                std::min(255, m_bgColor.Red() + 25),
                std::min(255, m_bgColor.Green() + 25),
                std::min(255, m_bgColor.Blue() + 25)
            );
            bubbleFg = m_fgColor;
        }
        
        // Draw bubble
        int bubbleWidth = size.GetWidth() - 20;
        int bubbleX = m_isUser ? 10 : 10;
        
        dc.SetBrush(wxBrush(bubbleBg));
        dc.DrawRoundedRectangle(bubbleX, 4, bubbleWidth, size.GetHeight() - 8, 10);
        
        // Draw text
        dc.SetFont(GetFont());
        dc.SetTextForeground(bubbleFg);
        
        wxString wrapped = WrapText(m_text, bubbleWidth - 20, dc);
        int y = 10;
        int x = bubbleX + 10;
        
        wxStringTokenizer tokenizer(wrapped, "\n");
        while (tokenizer.HasMoreTokens()) {
            wxString line = tokenizer.GetNextToken();
            dc.DrawText(line, x, y);
            y += dc.GetCharHeight();
        }
        
        // Role indicator (small label)
        wxFont smallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        dc.SetFont(smallFont);
        dc.SetTextForeground(wxColour(120, 120, 120));
        
        wxString label = m_isUser ? "You" : (m_isError ? "Error" : "Gemini");
        int labelX = m_isUser ? size.GetWidth() - dc.GetTextExtent(label).GetWidth() - 15 : 15;
        dc.DrawText(label, labelX, size.GetHeight() - 14);
    }
};

/**
 * Gemini AI Chat Widget.
 * Provides a conversational interface to Google's Gemini API.
 */
class GeminiChatWidget : public Widget {
public:
    WidgetInfo GetInfo() const override {
        WidgetInfo info;
        info.id = "core.geminiChat";
        info.name = "AI Chat";
        info.description = "Chat with Google Gemini AI";
        info.location = WidgetLocation::Sidebar;
        info.category = WidgetCategories::AI();
        info.priority = 55;
        info.showByDefault = true;
        return info;
    }

    wxWindow* CreateWindow(wxWindow* parent, WidgetContext& context) override {
        m_panel = new wxPanel(parent);
        m_context = &context;
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // Header
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("\U0001F916 AI Chat")); // 🤖
        wxFont headerFont = m_headerLabel->GetFont();
        headerFont.SetWeight(wxFONTWEIGHT_BOLD);
        headerFont.SetPointSize(11);
        m_headerLabel->SetFont(headerFont);
        headerSizer->Add(m_headerLabel, 1, wxALIGN_CENTER_VERTICAL);
        
        // Model selector
        m_modelChoice = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxSize(100, -1));
        for (const auto& model : AI::GeminiClient::GetAvailableModels()) {
            m_modelChoice->Append(wxString(model));
        }
        m_modelChoice->SetSelection(0);
        m_modelChoice->SetToolTip("Select AI model");
        headerSizer->Add(m_modelChoice, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
        
        // Clear button
        m_clearBtn = new wxButton(m_panel, wxID_ANY, wxT("\U0001F5D1"), wxDefaultPosition, wxSize(28, 24)); // 🗑
        m_clearBtn->SetToolTip("Clear conversation");
        headerSizer->Add(m_clearBtn, 0, wxLEFT, 5);
        
        mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);
        
        // Status label
        m_statusLabel = new wxStaticText(m_panel, wxID_ANY, "");
        wxFont statusFont = m_statusLabel->GetFont();
        statusFont.SetPointSize(8);
        m_statusLabel->SetFont(statusFont);
        mainSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        
        // Chat messages area
        m_chatPanel = new wxScrolledWindow(m_panel, wxID_ANY);
        m_chatPanel->SetScrollRate(0, 10);
        m_chatSizer = new wxBoxSizer(wxVERTICAL);
        m_chatPanel->SetSizer(m_chatSizer);
        mainSizer->Add(m_chatPanel, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);
        
        // Input area
        wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
        m_inputText = new wxTextCtrl(m_panel, wxID_ANY, "", wxDefaultPosition, 
                                     wxSize(-1, 60), wxTE_MULTILINE | wxTE_PROCESS_ENTER);
        m_inputText->SetHint("Type a message...");
        inputSizer->Add(m_inputText, 1, wxEXPAND | wxRIGHT, 5);
        
        m_sendBtn = new wxButton(m_panel, wxID_ANY, wxT("\u27A4"), wxDefaultPosition, wxSize(40, 60)); // ➤
        m_sendBtn->SetToolTip("Send message");
        inputSizer->Add(m_sendBtn, 0, wxEXPAND);
        
        mainSizer->Add(inputSizer, 0, wxEXPAND | wxALL, 8);
        
        // API key warning
        m_apiKeyWarning = new wxStaticText(m_panel, wxID_ANY, 
            wxT("\u26A0 Set ai.gemini.apiKey in config")); // ⚠
        m_apiKeyWarning->SetForegroundColour(wxColour(255, 180, 50));
        wxFont warningFont = m_apiKeyWarning->GetFont();
        warningFont.SetPointSize(8);
        m_apiKeyWarning->SetFont(warningFont);
        mainSizer->Add(m_apiKeyWarning, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        
        m_panel->SetSizer(mainSizer);
        
        // Bind events
        m_sendBtn->Bind(wxEVT_BUTTON, &GeminiChatWidget::OnSendMessage, this);
        m_clearBtn->Bind(wxEVT_BUTTON, &GeminiChatWidget::OnClearChat, this);
        m_modelChoice->Bind(wxEVT_CHOICE, &GeminiChatWidget::OnModelChanged, this);
        m_inputText->Bind(wxEVT_TEXT_ENTER, &GeminiChatWidget::OnSendMessage, this);
        m_inputText->Bind(wxEVT_KEY_DOWN, &GeminiChatWidget::OnKeyDown, this);
        
        // Timer for checking async responses
        m_responseTimer.Bind(wxEVT_TIMER, &GeminiChatWidget::OnResponseTimer, this);
        
        // Load config and check API key
        LoadConfig();
        UpdateApiKeyWarning();
        
        // Add welcome message
        AddMessageBubble("Hello! I'm Gemini, your AI assistant. How can I help you today?", false);
        
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
        m_chatPanel->SetBackgroundColour(m_bgColor);
        m_inputText->SetBackgroundColour(wxColour(
            std::min(255, m_bgColor.Red() + 15),
            std::min(255, m_bgColor.Green() + 15),
            std::min(255, m_bgColor.Blue() + 15)
        ));
        m_inputText->SetForegroundColour(m_fgColor);
        
        // Update all message bubbles
        for (wxWindow* child : m_chatPanel->GetChildren()) {
            if (auto* bubble = dynamic_cast<ChatMessageBubble*>(child)) {
                bubble->SetThemeColors(m_bgColor, m_fgColor);
            }
        }
        
        m_panel->Refresh();
    }

    std::vector<wxString> GetCommands() const override {
        return {
            "ai.chat.show",
            "ai.chat.hide",
            "ai.chat.toggle",
            "ai.chat.clear",
            "ai.chat.send",
            "ai.chat.configure"
        };
    }

    void RegisterCommands(WidgetContext& context) override {
        auto& registry = CommandRegistry::Instance();
        m_context = &context;
        
        auto makeCmd = [](const wxString& id, const wxString& title, 
                         const wxString& desc, Command::ExecuteFunc exec,
                         Command::EnabledFunc enabled = nullptr) {
            auto cmd = std::make_shared<Command>(id, title, "AI");
            cmd->SetDescription(desc);
            cmd->SetExecuteHandler(std::move(exec));
            if (enabled) cmd->SetEnabledHandler(std::move(enabled));
            return cmd;
        };
        
        GeminiChatWidget* self = this;
        
        registry.Register(makeCmd(
            "ai.chat.toggle", "Toggle AI Chat",
            "Show or hide the AI chat widget",
            [self](CommandContext& ctx) {
                self->ToggleVisibility(ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "ai.chat.show", "Show AI Chat",
            "Show the AI chat widget",
            [self](CommandContext& ctx) {
                self->SetVisible(true, ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "ai.chat.hide", "Hide AI Chat",
            "Hide the AI chat widget",
            [self](CommandContext& ctx) {
                self->SetVisible(false, ctx);
            }
        ));
        
        registry.Register(makeCmd(
            "ai.chat.clear", "Clear AI Chat",
            "Clear the conversation history",
            [self](CommandContext& ctx) {
                self->ClearConversation();
            }
        ));
        
        registry.Register(makeCmd(
            "ai.chat.configure", "Configure AI Chat",
            "Open AI configuration",
            [self](CommandContext& ctx) {
                self->OpenConfiguration();
            }
        ));
    }
    
    bool IsVisible() const {
        return m_panel && m_panel->IsShown();
    }
    
    void SetVisible(bool visible, CommandContext& ctx) {
        auto* frame = ctx.Get<MainFrame>("mainFrame");
        if (frame) {
            frame->ShowSidebarWidget("core.geminiChat", visible);
        }
    }
    
    void ToggleVisibility(CommandContext& ctx) {
        SetVisible(!IsVisible(), ctx);
    }
    
    void ClearConversation() {
        AI::GeminiClient::Instance().ClearConversation();
        
        // Clear UI
        m_chatSizer->Clear(true);
        m_chatPanel->Layout();
        m_chatPanel->Refresh();
        
        // Add welcome message
        AddMessageBubble("Chat cleared. How can I help you?", false);
        
        UpdateStatus("");
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
    wxStaticText* m_apiKeyWarning = nullptr;
    wxChoice* m_modelChoice = nullptr;
    wxButton* m_clearBtn = nullptr;
    wxButton* m_sendBtn = nullptr;
    wxScrolledWindow* m_chatPanel = nullptr;
    wxBoxSizer* m_chatSizer = nullptr;
    wxTextCtrl* m_inputText = nullptr;
    WidgetContext* m_context = nullptr;
    
    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);
    
    wxTimer m_responseTimer;
    std::atomic<bool> m_isLoading{false};
    
    // Thread-safe response queue
    std::mutex m_responseMutex;
    struct PendingResponse {
        wxString text;
        bool isError;
    };
    std::queue<PendingResponse> m_pendingResponses;
    
    void LoadConfig() {
        AI::GeminiClient::Instance().LoadFromConfig();
        
        // Set model choice to current model
        wxString currentModel = wxString(AI::GeminiClient::Instance().GetModel());
        int idx = m_modelChoice->FindString(currentModel);
        if (idx != wxNOT_FOUND) {
            m_modelChoice->SetSelection(idx);
        }
    }
    
    void UpdateApiKeyWarning() {
        bool hasKey = AI::GeminiClient::Instance().HasApiKey();
        m_apiKeyWarning->Show(!hasKey);
        m_sendBtn->Enable(hasKey);
        m_panel->Layout();
    }
    
    void UpdateStatus(const wxString& status) {
        if (m_statusLabel) {
            m_statusLabel->SetLabel(status);
        }
    }
    
    void AddMessageBubble(const wxString& text, bool isUser, bool isError = false) {
        auto* bubble = new ChatMessageBubble(m_chatPanel, text, isUser, isError);
        bubble->SetThemeColors(m_bgColor, m_fgColor);
        m_chatSizer->Add(bubble, 0, wxEXPAND | wxALL, 5);
        m_chatPanel->Layout();
        m_chatPanel->FitInside();
        
        // Scroll to bottom
        m_chatPanel->Scroll(-1, m_chatPanel->GetVirtualSize().GetHeight());
    }
    
    void OnSendMessage(wxCommandEvent& event) {
        SendCurrentMessage();
    }
    
    void OnKeyDown(wxKeyEvent& event) {
        // Send on Enter (without Shift)
        if (event.GetKeyCode() == WXK_RETURN && !event.ShiftDown()) {
            SendCurrentMessage();
        } else {
            event.Skip();
        }
    }
    
    void SendCurrentMessage() {
        wxString message = m_inputText->GetValue().Trim().Trim(false);
        if (message.IsEmpty() || m_isLoading) {
            return;
        }
        
        // Check API key
        if (!AI::GeminiClient::Instance().HasApiKey()) {
            AddMessageBubble("Please configure your API key first (ai.gemini.apiKey in config)", false, true);
            return;
        }
        
        // Clear input
        m_inputText->Clear();
        
        // Add user message bubble
        AddMessageBubble(message, true);
        
        // Show loading state
        m_isLoading = true;
        UpdateStatus(wxT("\U0001F504 Thinking...")); // 🔄
        m_sendBtn->Enable(false);
        
        // Send message in background thread
        std::string msgStr = message.ToStdString();
        std::thread([this, msgStr]() {
            AI::GeminiResponse response = AI::GeminiClient::Instance().SendMessage(msgStr);
            
            // Queue response for UI thread
            {
                std::lock_guard<std::mutex> lock(m_responseMutex);
                if (response.isOk()) {
                    m_pendingResponses.push({wxString(response.text), false});
                } else {
                    m_pendingResponses.push({wxString(response.error), true});
                }
            }
            
            m_isLoading = false;
        }).detach();
        
        // Start timer to check for responses
        m_responseTimer.Start(100);
    }
    
    void OnResponseTimer(wxTimerEvent& event) {
        // Check for pending responses
        PendingResponse response;
        bool hasResponse = false;
        
        {
            std::lock_guard<std::mutex> lock(m_responseMutex);
            if (!m_pendingResponses.empty()) {
                response = m_pendingResponses.front();
                m_pendingResponses.pop();
                hasResponse = true;
            }
        }
        
        if (hasResponse) {
            AddMessageBubble(response.text, false, response.isError);
            UpdateStatus("");
            m_sendBtn->Enable(AI::GeminiClient::Instance().HasApiKey());
            
            if (m_pendingResponses.empty() && !m_isLoading) {
                m_responseTimer.Stop();
            }
        } else if (!m_isLoading) {
            m_responseTimer.Stop();
            UpdateStatus("");
            m_sendBtn->Enable(AI::GeminiClient::Instance().HasApiKey());
        }
    }
    
    void OnClearChat(wxCommandEvent& event) {
        ClearConversation();
    }
    
    void OnModelChanged(wxCommandEvent& event) {
        wxString model = m_modelChoice->GetStringSelection();
        AI::GeminiClient::Instance().SetModel(model.ToStdString());
        AI::GeminiClient::Instance().SaveToConfig();
    }
};

} // namespace BuiltinWidgets

#endif // GEMINI_CHAT_WIDGET_H
