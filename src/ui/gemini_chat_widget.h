#ifndef GEMINI_CHAT_WIDGET_H
#define GEMINI_CHAT_WIDGET_H

#include "widget.h"
#include "../theme/theme.h"
#include "../config/config.h"
#include "../commands/command.h"
#include "../commands/command_registry.h"
#include "../ai/gemini_client.h"
#include "../mcp/mcp.h"
#include "../mcp/mcp_filesystem.h"
#include "../mcp/mcp_terminal.h"
#include "../mcp/mcp_code_index.h"
#include <wx/dcbuffer.h>
#include <wx/timer.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/wrapsizer.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/tokenzr.h>
#include <wx/clipbrd.h>
#include <wx/menu.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

// Forward declaration
class MainFrame;

namespace BuiltinWidgets {

/**
 * Represents a single styled word for rendering.
 */
struct StyledWord {
    wxString text;
    bool bold = false;
    bool italic = false;
    bool code = false;
    bool header = false;
    int headerLevel = 0;
    bool isSpace = false;
};

/**
 * Represents a span of styled text within a line.
 */
struct TextSpan {
    wxString text;
    bool bold = false;
    bool italic = false;
    bool code = false;
    bool header = false;
    int headerLevel = 0;
};

/**
 * Represents a parsed line with styling information.
 */
struct ParsedLine {
    std::vector<TextSpan> spans;
    bool isCodeBlock = false;
    bool isBulletList = false;
    bool isNumberedList = false;
    int listNumber = 0;
    int indentLevel = 0;
};

/**
 * Custom panel for displaying a single chat message bubble with markdown support.
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
        
        // Parse markdown on construction
        ParseMarkdown();
        
        // Initial height calculation will be done on first size event
        SetMinSize(wxSize(-1, 36));
        
        Bind(wxEVT_PAINT, &ChatMessageBubble::OnPaint, this);
        Bind(wxEVT_SIZE, &ChatMessageBubble::OnSize, this);
        Bind(wxEVT_RIGHT_DOWN, &ChatMessageBubble::OnRightClick, this);
    }
    
    void SetThemeColors(const wxColour& bg, const wxColour& fg) {
        m_bgColor = bg;
        m_fgColor = fg;
        Refresh();
    }
    
    void SetText(const wxString& text) {
        m_text = text;
        ParseMarkdown();
        RecalculateHeight();
        GetParent()->Layout();
        Refresh();
    }
    
    wxString GetText() const {
        return m_text;
    }
    
private:
    void OnRightClick(wxMouseEvent& event) {
        wxMenu menu;
        menu.Append(wxID_COPY, "Copy");
        
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (wxTheClipboard->Open()) {
                wxTheClipboard->SetData(new wxTextDataObject(m_text));
                wxTheClipboard->Close();
            }
        }, wxID_COPY);
        
        PopupMenu(&menu, event.GetPosition());
    }
    
private:
    wxString m_text;
    bool m_isUser;
    bool m_isError;
    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);
    int m_lastWidth = 0;
    std::vector<ParsedLine> m_parsedLines;
    bool m_inCodeBlock = false;
    
    /**
     * Parse inline markdown formatting (bold, italic, code) within a line.
     */
    std::vector<TextSpan> ParseInlineMarkdown(const wxString& text, bool isHeader = false, int headerLevel = 0) {
        std::vector<TextSpan> spans;
        wxString current;
        bool inBold = false;
        bool inItalic = false;
        bool inCode = false;
        
        size_t i = 0;
        while (i < text.Length()) {
            // Check for inline code (backtick)
            if (text[i] == '`' && !inCode) {
                if (!current.IsEmpty()) {
                    TextSpan span;
                    span.text = current;
                    span.bold = inBold;
                    span.italic = inItalic;
                    span.header = isHeader;
                    span.headerLevel = headerLevel;
                    spans.push_back(span);
                    current.Clear();
                }
                inCode = true;
                i++;
                continue;
            }
            if (text[i] == '`' && inCode) {
                TextSpan span;
                span.text = current;
                span.code = true;
                span.header = isHeader;
                span.headerLevel = headerLevel;
                spans.push_back(span);
                current.Clear();
                inCode = false;
                i++;
                continue;
            }
            
            // Check for bold (**text**)
            if (!inCode && i + 1 < text.Length() && text[i] == '*' && text[i+1] == '*') {
                if (!current.IsEmpty()) {
                    TextSpan span;
                    span.text = current;
                    span.bold = inBold;
                    span.italic = inItalic;
                    span.header = isHeader;
                    span.headerLevel = headerLevel;
                    spans.push_back(span);
                    current.Clear();
                }
                inBold = !inBold;
                i += 2;
                continue;
            }
            
            // Check for italic (*text* or _text_) - but not ** which is bold
            if (!inCode && (text[i] == '*' || text[i] == '_')) {
                bool isBoldMarker = (i + 1 < text.Length() && text[i+1] == text[i]);
                if (!isBoldMarker) {
                    if (!current.IsEmpty()) {
                        TextSpan span;
                        span.text = current;
                        span.bold = inBold;
                        span.italic = inItalic;
                        span.header = isHeader;
                        span.headerLevel = headerLevel;
                        spans.push_back(span);
                        current.Clear();
                    }
                    inItalic = !inItalic;
                    i++;
                    continue;
                }
            }
            
            current += text[i];
            i++;
        }
        
        // Add remaining text
        if (!current.IsEmpty()) {
            TextSpan span;
            span.text = current;
            span.bold = inBold;
            span.italic = inItalic;
            span.code = inCode;
            span.header = isHeader;
            span.headerLevel = headerLevel;
            spans.push_back(span);
        }
        
        // If no spans were created, add an empty one
        if (spans.empty()) {
            TextSpan span;
            span.text = "";
            spans.push_back(span);
        }
        
        return spans;
    }
    
    /**
     * Parse the entire text into lines with markdown formatting.
     */
    void ParseMarkdown() {
        m_parsedLines.clear();
        m_inCodeBlock = false;
        
        wxStringTokenizer tokenizer(m_text, "\n", wxTOKEN_RET_EMPTY);
        
        while (tokenizer.HasMoreTokens()) {
            wxString line = tokenizer.GetNextToken();
            ParsedLine parsedLine;
            
            // Check for code block start/end
            if (line.StartsWith("```")) {
                m_inCodeBlock = !m_inCodeBlock;
                if (m_inCodeBlock) {
                    // Starting a code block - this line might have a language specifier
                    parsedLine.isCodeBlock = true;
                    TextSpan span;
                    span.text = line.Mid(3).Trim(); // Language identifier if any
                    span.code = true;
                    parsedLine.spans.push_back(span);
                } else {
                    // Ending a code block
                    parsedLine.isCodeBlock = true;
                    TextSpan span;
                    span.text = "";
                    span.code = true;
                    parsedLine.spans.push_back(span);
                }
                m_parsedLines.push_back(parsedLine);
                continue;
            }
            
            // If we're inside a code block, treat as literal code
            if (m_inCodeBlock) {
                parsedLine.isCodeBlock = true;
                TextSpan span;
                span.text = line;
                span.code = true;
                parsedLine.spans.push_back(span);
                m_parsedLines.push_back(parsedLine);
                continue;
            }
            
            // Check for headers
            int headerLevel = 0;
            if (line.StartsWith("### ")) {
                headerLevel = 3;
                line = line.Mid(4);
            } else if (line.StartsWith("## ")) {
                headerLevel = 2;
                line = line.Mid(3);
            } else if (line.StartsWith("# ")) {
                headerLevel = 1;
                line = line.Mid(2);
            }
            
            // Check for bullet lists
            wxString trimmedLine = line;
            int indent = 0;
            while (trimmedLine.StartsWith("  ")) {
                indent++;
                trimmedLine = trimmedLine.Mid(2);
            }
            
            if (trimmedLine.StartsWith("- ") || trimmedLine.StartsWith("* ")) {
                parsedLine.isBulletList = true;
                parsedLine.indentLevel = indent;
                line = trimmedLine.Mid(2);
            }
            // Check for numbered lists
            else if (trimmedLine.Length() > 2) {
                size_t dotPos = trimmedLine.find('.');
                if (dotPos != wxString::npos && dotPos < 4) {
                    wxString numStr = trimmedLine.Left(dotPos);
                    long num;
                    if (numStr.ToLong(&num) && num > 0 && num < 100) {
                        if (trimmedLine.Length() > dotPos + 1 && trimmedLine[dotPos + 1] == ' ') {
                            parsedLine.isNumberedList = true;
                            parsedLine.listNumber = static_cast<int>(num);
                            parsedLine.indentLevel = indent;
                            line = trimmedLine.Mid(dotPos + 2);
                        }
                    }
                }
            }
            
            // Parse inline formatting
            parsedLine.spans = ParseInlineMarkdown(line, headerLevel > 0, headerLevel);
            m_parsedLines.push_back(parsedLine);
        }
    }
    
    void RecalculateHeight() {
        wxClientDC dc(this);
        wxFont baseFont = GetFont();
        dc.SetFont(baseFont);
        int baseLineHeight = dc.GetCharHeight();
        int width = GetClientSize().GetWidth();
        if (width <= 0) width = 280; // Fallback
        
        int bubbleWidth = width - 20; // Account for margins
        int contentWidth = bubbleWidth - 20;
        
        // Create fonts for measurement
        wxFont boldFont = baseFont;
        boldFont.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont italicFont = baseFont;
        italicFont.SetStyle(wxFONTSTYLE_ITALIC);
        wxFont boldItalicFont = boldFont;
        boldItalicFont.SetStyle(wxFONTSTYLE_ITALIC);
        wxFont codeFont(baseFont.GetPointSize(), wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxFont h1Font = baseFont;
        h1Font.SetPointSize(baseFont.GetPointSize() + 4);
        h1Font.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont h2Font = baseFont;
        h2Font.SetPointSize(baseFont.GetPointSize() + 2);
        h2Font.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont h3Font = baseFont;
        h3Font.SetPointSize(baseFont.GetPointSize() + 1);
        h3Font.SetWeight(wxFONTWEIGHT_BOLD);
        
        int totalHeight = 0;
        
        for (const auto& line : m_parsedLines) {
            int lineHeight = baseLineHeight;
            
            // Headers are taller
            if (!line.spans.empty() && line.spans[0].header) {
                int level = line.spans[0].headerLevel;
                if (level == 1) lineHeight = baseLineHeight * 3 / 2;
                else if (level == 2) lineHeight = baseLineHeight * 5 / 4;
                else lineHeight = baseLineHeight * 9 / 8;
            }
            
            // Code blocks
            if (line.isCodeBlock) {
                if (!line.spans.empty() && !line.spans[0].text.IsEmpty()) {
                    totalHeight += lineHeight + 2;
                } else {
                    totalHeight += 4;
                }
                continue;
            }
            
            // Build word list for this line and calculate with proper fonts
            std::vector<StyledWord> words;
            for (const auto& span : line.spans) {
                if (span.text.IsEmpty()) continue;
                
                wxString currentWord;
                for (size_t i = 0; i < span.text.Length(); ++i) {
                    wxChar c = span.text[i];
                    if (c == ' ') {
                        if (!currentWord.IsEmpty()) {
                            StyledWord sw;
                            sw.text = currentWord;
                            sw.bold = span.bold;
                            sw.italic = span.italic;
                            sw.code = span.code;
                            sw.header = span.header;
                            sw.headerLevel = span.headerLevel;
                            words.push_back(sw);
                            currentWord.Clear();
                        }
                        StyledWord space;
                        space.text = " ";
                        space.isSpace = true;
                        space.code = span.code;
                        words.push_back(space);
                    } else {
                        currentWord += c;
                    }
                }
                if (!currentWord.IsEmpty()) {
                    StyledWord sw;
                    sw.text = currentWord;
                    sw.bold = span.bold;
                    sw.italic = span.italic;
                    sw.code = span.code;
                    sw.header = span.header;
                    sw.headerLevel = span.headerLevel;
                    words.push_back(sw);
                }
            }
            
            // Calculate number of visual lines by simulating wrapping
            int baseX = 0;
            if (line.isBulletList || line.isNumberedList) {
                baseX = 15 + (line.indentLevel * 15);
            }
            int maxX = contentWidth;
            int x = baseX;
            int lineCount = 1;
            
            for (size_t wi = 0; wi < words.size(); ++wi) {
                const auto& word = words[wi];
                
                // Set font for width calculation
                if (word.header) {
                    if (word.headerLevel == 1) dc.SetFont(h1Font);
                    else if (word.headerLevel == 2) dc.SetFont(h2Font);
                    else dc.SetFont(h3Font);
                } else if (word.code) {
                    dc.SetFont(codeFont);
                } else if (word.bold && word.italic) {
                    dc.SetFont(boldItalicFont);
                } else if (word.bold) {
                    dc.SetFont(boldFont);
                } else if (word.italic) {
                    dc.SetFont(italicFont);
                } else {
                    dc.SetFont(baseFont);
                }
                
                wxSize wordExtent = dc.GetTextExtent(word.text);
                
                if (!word.isSpace && x + wordExtent.GetWidth() > maxX && x > baseX) {
                    lineCount++;
                    x = baseX;
                }
                
                if (!(word.isSpace && x == baseX)) {
                    x += wordExtent.GetWidth();
                }
            }
            
            totalHeight += lineCount * lineHeight;
            
            // Add extra spacing after headers
            if (!line.spans.empty() && line.spans[0].header) {
                totalHeight += 4;
            }
        }
        
        int height = std::max(36, totalHeight + 28); // 28 for padding + role label
        SetMinSize(wxSize(-1, height));
    }
    
    void OnSize(wxSizeEvent& event) {
        int newWidth = event.GetSize().GetWidth();
        if (newWidth != m_lastWidth && newWidth > 0) {
            m_lastWidth = newWidth;
            RecalculateHeight();
            // Request parent to re-layout since our min height may have changed
            if (GetParent()) {
                GetParent()->Layout();
            }
        }
        event.Skip();
    }
    
    wxString WrapTextSimple(const wxString& text, int maxWidth, wxDC& dc) {
        wxString result;
        wxString line;
        wxString word;
        
        for (size_t i = 0; i <= text.Length(); ++i) {
            wxChar c = (i < text.Length()) ? text[i].GetValue() : static_cast<wxChar>(' ');
            
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
        wxColour codeBg;
        wxColour codeFg;
        
        if (m_isError) {
            bubbleBg = wxColour(120, 40, 40);
            bubbleFg = wxColour(255, 180, 180);
            codeBg = wxColour(100, 30, 30);
            codeFg = wxColour(255, 200, 150);
        } else if (m_isUser) {
            bubbleBg = wxColour(59, 130, 246); // Blue for user
            bubbleFg = wxColour(255, 255, 255);
            codeBg = wxColour(49, 110, 200);
            codeFg = wxColour(255, 255, 200);
        } else {
            // Model response - adapt to theme
            bubbleBg = wxColour(
                std::min(255, m_bgColor.Red() + 25),
                std::min(255, m_bgColor.Green() + 25),
                std::min(255, m_bgColor.Blue() + 25)
            );
            bubbleFg = m_fgColor;
            codeBg = wxColour(
                std::max(0, m_bgColor.Red() - 10),
                std::max(0, m_bgColor.Green() - 10),
                std::max(0, m_bgColor.Blue() - 10)
            );
            codeFg = wxColour(220, 180, 100); // Warm code color
        }
        
        // Draw bubble
        int bubbleWidth = size.GetWidth() - 20;
        int bubbleX = 10;
        
        dc.SetBrush(wxBrush(bubbleBg));
        dc.DrawRoundedRectangle(bubbleX, 4, bubbleWidth, size.GetHeight() - 8, 10);
        
        // Draw markdown content
        wxFont baseFont = GetFont();
        wxFont boldFont = baseFont;
        boldFont.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont italicFont = baseFont;
        italicFont.SetStyle(wxFONTSTYLE_ITALIC);
        wxFont boldItalicFont = boldFont;
        boldItalicFont.SetStyle(wxFONTSTYLE_ITALIC);
        wxFont codeFont(baseFont.GetPointSize(), wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxFont h1Font = baseFont;
        h1Font.SetPointSize(baseFont.GetPointSize() + 4);
        h1Font.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont h2Font = baseFont;
        h2Font.SetPointSize(baseFont.GetPointSize() + 2);
        h2Font.SetWeight(wxFONTWEIGHT_BOLD);
        wxFont h3Font = baseFont;
        h3Font.SetPointSize(baseFont.GetPointSize() + 1);
        h3Font.SetWeight(wxFONTWEIGHT_BOLD);
        
        int y = 10;
        int baseX = bubbleX + 10;
        int contentWidth = bubbleWidth - 20;
        int baseLineHeight = dc.GetCharHeight();
        
        for (const auto& line : m_parsedLines) {
            int x = baseX;
            int lineHeight = baseLineHeight;
            
            // Handle list indentation
            if (line.isBulletList || line.isNumberedList) {
                x += line.indentLevel * 15;
                dc.SetFont(baseFont);
                dc.SetTextForeground(bubbleFg);
                if (line.isBulletList) {
                    dc.DrawText(wxT("\u2022"), x, y); // Bullet point
                } else {
                    dc.DrawText(wxString::Format("%d.", line.listNumber), x, y);
                }
                x += 15;
            }
            
            // Code block handling
            if (line.isCodeBlock && !line.spans.empty() && !line.spans[0].text.IsEmpty()) {
                dc.SetBrush(wxBrush(codeBg));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(baseX, y - 2, contentWidth, baseLineHeight + 4);
                dc.SetFont(codeFont);
                dc.SetTextForeground(codeFg);
                dc.DrawText(line.spans[0].text, baseX + 5, y);
                y += lineHeight + 2;
                continue;
            } else if (line.isCodeBlock) {
                // Empty code block line (start/end marker)
                y += 4;
                continue;
            }
            
            // Render each span in the line
            // First, build a list of styled words from all spans
            std::vector<StyledWord> words;
            for (const auto& span : line.spans) {
                if (span.text.IsEmpty()) continue;
                
                // Split span text into words, preserving spaces
                wxString currentWord;
                for (size_t i = 0; i < span.text.Length(); ++i) {
                    wxChar c = span.text[i];
                    if (c == ' ') {
                        if (!currentWord.IsEmpty()) {
                            StyledWord sw;
                            sw.text = currentWord;
                            sw.bold = span.bold;
                            sw.italic = span.italic;
                            sw.code = span.code;
                            sw.header = span.header;
                            sw.headerLevel = span.headerLevel;
                            words.push_back(sw);
                            currentWord.Clear();
                        }
                        // Add space as its own word
                        StyledWord space;
                        space.text = " ";
                        space.isSpace = true;
                        space.bold = span.bold;
                        space.italic = span.italic;
                        space.code = span.code;
                        space.header = span.header;
                        space.headerLevel = span.headerLevel;
                        words.push_back(space);
                    } else {
                        currentWord += c;
                    }
                }
                if (!currentWord.IsEmpty()) {
                    StyledWord sw;
                    sw.text = currentWord;
                    sw.bold = span.bold;
                    sw.italic = span.italic;
                    sw.code = span.code;
                    sw.header = span.header;
                    sw.headerLevel = span.headerLevel;
                    words.push_back(sw);
                }
            }
            
            // Helper lambda to set font for a styled word
            auto setFontForWord = [&](const StyledWord& word) {
                if (word.header) {
                    if (word.headerLevel == 1) {
                        dc.SetFont(h1Font);
                        lineHeight = h1Font.GetPointSize() + 8;
                    } else if (word.headerLevel == 2) {
                        dc.SetFont(h2Font);
                        lineHeight = h2Font.GetPointSize() + 6;
                    } else {
                        dc.SetFont(h3Font);
                        lineHeight = h3Font.GetPointSize() + 4;
                    }
                } else if (word.code) {
                    dc.SetFont(codeFont);
                } else if (word.bold && word.italic) {
                    dc.SetFont(boldItalicFont);
                } else if (word.bold) {
                    dc.SetFont(boldFont);
                } else if (word.italic) {
                    dc.SetFont(italicFont);
                } else {
                    dc.SetFont(baseFont);
                }
            };
            
            // Calculate the maximum x position before wrapping
            int maxX = baseX + contentWidth;
            if (line.isBulletList || line.isNumberedList) {
                // Account for list marker space
            }
            
            // Render words with proper wrapping
            for (size_t wi = 0; wi < words.size(); ++wi) {
                const auto& word = words[wi];
                
                setFontForWord(word);
                wxSize wordExtent = dc.GetTextExtent(word.text);
                
                // Check if we need to wrap (skip wrapping for leading spaces on new line)
                if (!word.isSpace && x + wordExtent.GetWidth() > maxX && x > baseX + (line.isBulletList || line.isNumberedList ? 15 + line.indentLevel * 15 : 0)) {
                    // Wrap to next line
                    y += lineHeight;
                    x = baseX;
                    if (line.isBulletList || line.isNumberedList) {
                        x += 15 + (line.indentLevel * 15);
                    }
                }
                
                // Skip leading spaces at the start of a wrapped line
                if (word.isSpace && x == baseX + (line.isBulletList || line.isNumberedList ? 15 + line.indentLevel * 15 : 0)) {
                    continue;
                }
                
                // Set colors and draw
                if (word.code) {
                    dc.SetTextForeground(codeFg);
                    if (!word.isSpace) {
                        // Draw code background
                        dc.SetBrush(wxBrush(codeBg));
                        dc.SetPen(*wxTRANSPARENT_PEN);
                        dc.DrawRoundedRectangle(x - 2, y - 1, wordExtent.GetWidth() + 4, wordExtent.GetHeight() + 2, 3);
                    }
                } else {
                    dc.SetTextForeground(bubbleFg);
                }
                
                dc.DrawText(word.text, x, y);
                x += wordExtent.GetWidth();
            }
            
            y += lineHeight;
            
            // Extra spacing after headers
            if (!line.spans.empty() && line.spans[0].header) {
                y += 4;
            }
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
#ifdef __WXMSW__
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("[AI] AI Chat"));
#else
        m_headerLabel = new wxStaticText(m_panel, wxID_ANY, wxT("\U0001F916 AI Chat")); // 🤖
#endif
        wxFont headerFont = m_headerLabel->GetFont();
        headerFont.SetWeight(wxFONTWEIGHT_BOLD);
        headerFont.SetPointSize(11);
        m_headerLabel->SetFont(headerFont);
        headerSizer->Add(m_headerLabel, 1, wxALIGN_CENTER_VERTICAL);
        
        // Provider selector
        m_providerChoice = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxSize(70, -1));
        m_providerChoice->Append("Gemini");
        m_providerChoice->Append("Cortex");
        m_providerChoice->SetSelection(AI::GeminiClient::Instance().GetProvider() == AI::AIProvider::Cortex ? 1 : 0);
        m_providerChoice->SetToolTip("Select AI provider");
        headerSizer->Add(m_providerChoice, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
        
        // Model selector
        m_modelChoice = new wxChoice(m_panel, wxID_ANY, wxDefaultPosition, wxSize(100, -1));
        PopulateModelList(); // Start with fallback, will refresh when dropdown opens
        m_modelChoice->SetToolTip("Select AI model (click refresh to fetch from API)");
        headerSizer->Add(m_modelChoice, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);
        
        // Refresh models button
#ifdef __WXMSW__
        m_refreshModelsBtn = new wxButton(m_panel, wxID_ANY, wxT("R"), wxDefaultPosition, wxSize(28, 24));
#else
        m_refreshModelsBtn = new wxButton(m_panel, wxID_ANY, wxT("\U0001F504"), wxDefaultPosition, wxSize(28, 24)); // 🔄
#endif
        m_refreshModelsBtn->SetToolTip("Refresh available models from API");
        headerSizer->Add(m_refreshModelsBtn, 0, wxLEFT, 2);
        
        // Clear button
#ifdef __WXMSW__
        m_clearBtn = new wxButton(m_panel, wxID_ANY, wxT("X"), wxDefaultPosition, wxSize(28, 24));
#else
        m_clearBtn = new wxButton(m_panel, wxID_ANY, wxT("\U0001F5D1"), wxDefaultPosition, wxSize(28, 24)); // 🗑
#endif
        m_clearBtn->SetToolTip("Clear conversation");
        headerSizer->Add(m_clearBtn, 0, wxLEFT, 5);
        
        mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 8);
        
        // MCP tools toggle row
        wxBoxSizer* mcpSizer = new wxBoxSizer(wxHORIZONTAL);
#ifdef __WXMSW__
        m_mcpCheckbox = new wxCheckBox(m_panel, wxID_ANY, wxT("[F] File Access"));
#else
        m_mcpCheckbox = new wxCheckBox(m_panel, wxID_ANY, wxT("\U0001F4C1 File Access")); // 📁
#endif
        m_mcpCheckbox->SetValue(true);
        m_mcpCheckbox->SetToolTip("Allow AI to read files in the current workspace");
        mcpSizer->Add(m_mcpCheckbox, 0, wxALIGN_CENTER_VERTICAL);
        
        m_mcpStatusLabel = new wxStaticText(m_panel, wxID_ANY, "");
        wxFont mcpStatusFont = m_mcpStatusLabel->GetFont();
        mcpStatusFont.SetPointSize(8);
        m_mcpStatusLabel->SetFont(mcpStatusFont);
        m_mcpStatusLabel->SetForegroundColour(wxColour(100, 180, 100));
        mcpSizer->Add(m_mcpStatusLabel, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
        
        mainSizer->Add(mcpSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
        
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
        m_refreshModelsBtn->Bind(wxEVT_BUTTON, &GeminiChatWidget::OnRefreshModels, this);
        m_providerChoice->Bind(wxEVT_CHOICE, &GeminiChatWidget::OnProviderChanged, this);
        m_modelChoice->Bind(wxEVT_CHOICE, &GeminiChatWidget::OnModelChanged, this);
        m_mcpCheckbox->Bind(wxEVT_CHECKBOX, &GeminiChatWidget::OnMCPToggle, this);
        m_inputText->Bind(wxEVT_TEXT_ENTER, &GeminiChatWidget::OnSendMessage, this);
        m_inputText->Bind(wxEVT_KEY_DOWN, &GeminiChatWidget::OnKeyDown, this);
        
        // Bind resize event to recalculate bubble heights when splitter changes
        m_chatPanel->Bind(wxEVT_SIZE, &GeminiChatWidget::OnChatPanelResize, this);
        
        // Timer for checking async responses
        m_responseTimer.Bind(wxEVT_TIMER, &GeminiChatWidget::OnResponseTimer, this);
        
        // Load config and check API key
        LoadConfig();
        UpdateApiKeyWarning();
        
        // Initialize MCP filesystem provider
        InitializeMCP();
        
        // Add welcome message based on provider
        wxString welcomeMsg = (AI::GeminiClient::Instance().GetProvider() == AI::AIProvider::Cortex)
            ? "Hello! I'm your AI assistant (via Cortex). How can I help you today?"
            : "Hello! I'm your AI assistant (via Gemini). How can I help you today?";
        AddMessageBubble(welcomeMsg, false);
        
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
    wxStaticText* m_mcpStatusLabel = nullptr;
    wxChoice* m_providerChoice = nullptr;
    wxChoice* m_modelChoice = nullptr;
    wxCheckBox* m_mcpCheckbox = nullptr;
    wxButton* m_clearBtn = nullptr;
    wxButton* m_refreshModelsBtn = nullptr;
    wxButton* m_sendBtn = nullptr;
    wxScrolledWindow* m_chatPanel = nullptr;
    wxBoxSizer* m_chatSizer = nullptr;
    wxTextCtrl* m_inputText = nullptr;
    WidgetContext* m_context = nullptr;
    
    wxColour m_bgColor = wxColour(30, 30, 30);
    wxColour m_fgColor = wxColour(220, 220, 220);
    
    wxTimer m_responseTimer;
    std::atomic<bool> m_isLoading{false};
    
    // MCP providers
    std::shared_ptr<MCP::FilesystemProvider> m_fsProvider;
    std::shared_ptr<MCP::TerminalProvider> m_terminalProvider;
    std::shared_ptr<MCP::CodeIndexProvider> m_codeIndexProvider;
    
    // Thread-safe response queue
    std::mutex m_responseMutex;
    struct PendingResponse {
        wxString text;
        bool isError;
        bool isToolCall;
        wxString toolName;
        wxString toolArgs;
    };
    std::queue<PendingResponse> m_pendingResponses;
    
    /**
     * Load SSH configuration from global settings.
     */
    static MCP::TerminalSshConfig LoadTerminalSshConfig() {
        auto& config = Config::Instance();
        MCP::TerminalSshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.forwardAgent = config.GetBool("ssh.forwardAgent", false);
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        return ssh;
    }
    
    static MCP::FilesystemSshConfig LoadFilesystemSshConfig() {
        auto& config = Config::Instance();
        MCP::FilesystemSshConfig ssh;
        ssh.enabled = config.GetBool("ssh.enabled", false);
        ssh.host = config.GetString("ssh.host", "").ToStdString();
        ssh.port = config.GetInt("ssh.port", 22);
        ssh.user = config.GetString("ssh.user", "").ToStdString();
        ssh.identityFile = config.GetString("ssh.identityFile", "").ToStdString();
        ssh.extraOptions = config.GetString("ssh.extraOptions", "").ToStdString();
        ssh.connectionTimeout = config.GetInt("ssh.connectionTimeout", 30);
        return ssh;
    }
    
    /**
     * Initialize MCP providers with current working directory.
     * Applies SSH configuration if enabled.
     */
    void InitializeMCP() {
        auto& config = Config::Instance();
        bool sshEnabled = config.GetBool("ssh.enabled", false);
        
        // Determine working directory - local or remote
        std::string workDir;
        if (sshEnabled) {
            // Use remote path when SSH is enabled
            workDir = config.GetString("ssh.remotePath", "~").ToStdString();
            
            // Expand ~ to actual home directory path via SSH
            MCP::FilesystemSshConfig tempSsh = LoadFilesystemSshConfig();
            workDir = tempSsh.expandRemotePath(workDir);
        } else {
            workDir = wxGetCwd().ToStdString();
        }
        
        // Create filesystem provider
        m_fsProvider = std::make_shared<MCP::FilesystemProvider>(workDir);
        if (sshEnabled) {
            m_fsProvider->setSshConfig(LoadFilesystemSshConfig());
        }
        MCP::Registry::Instance().registerProvider(m_fsProvider);
        
        // Create terminal provider
        m_terminalProvider = std::make_shared<MCP::TerminalProvider>(workDir);
        if (sshEnabled) {
            m_terminalProvider->setSshConfig(LoadTerminalSshConfig());
        }
        MCP::Registry::Instance().registerProvider(m_terminalProvider);
        
        // Create code index provider (will be connected to SymbolsWidget later)
        m_codeIndexProvider = std::make_shared<MCP::CodeIndexProvider>();
        if (sshEnabled) {
            MCP::CodeIndexSshConfig codeIndexSsh;
            codeIndexSsh.enabled = true;
            codeIndexSsh.host = config.GetString("ssh.host", "").ToStdString();
            codeIndexSsh.remotePath = workDir;
            m_codeIndexProvider->setSshConfig(codeIndexSsh);
        }
        MCP::Registry::Instance().registerProvider(m_codeIndexProvider);
        
        // Enable MCP in Gemini client
        AI::GeminiClient::Instance().SetMCPEnabled(true);
        
        // Set system instruction to inform the AI about available tools
        std::string locationInfo = sshEnabled 
            ? "Remote workspace via SSH: " + config.GetString("ssh.host", "").ToStdString() + ":" + workDir
            : "Local workspace: " + workDir;
            
        std::string systemInstruction = 
            "You are a helpful AI assistant integrated into a code editor. "
            "You have access to the user's workspace files, terminal, and code index through several tools:\n\n"
            "FILESYSTEM TOOLS:\n"
            "- fs_list_directory: List files and folders in a directory\n"
            "- fs_read_file: Read the complete contents of a file\n"
            "- fs_read_file_lines: Read specific line ranges from a file\n"
            "- fs_get_file_info: Get metadata about a file (size, type, line count)\n"
            "- fs_search_files: Search for files by name pattern (e.g., '*.cpp')\n"
            "- fs_grep: Search for text content within files\n\n"
            "TERMINAL TOOLS:\n"
            "- terminal_execute: Execute shell commands (build, run scripts, git, etc.)\n"
            "- terminal_get_shell_info: Get info about the current shell environment\n"
            "- terminal_get_env: Get environment variable values\n"
            "- terminal_which: Find the path of an executable\n"
            "- terminal_list_processes: List running processes\n\n"
            "CODE INDEX TOOLS (powered by clangd):\n"
            "- code_search_symbols: Search for functions, classes, variables by name\n"
            "- code_list_file_symbols: List all symbols defined in a specific file\n"
            "- code_list_functions: List all functions/methods in the workspace\n"
            "- code_list_classes: List all classes and structs in the workspace\n"
            "- code_get_index_status: Check if code indexing is complete\n\n"
            "When the user asks about their code, project structure, or file contents, "
            "USE THESE TOOLS to read and explore their files. Don't say you can't access files - you can! "
            "When the user asks about code structure, functions, or classes, use the code index tools first "
            "for faster and more accurate results. "
            "When the user asks you to run commands, build code, or execute scripts, use the terminal tools.\n\n"
            + locationInfo;
        
        AI::GeminiClient::Instance().SetSystemInstruction(systemInstruction);
        
        UpdateMCPStatus();
    }
    
    /**
     * Update MCP status label.
     */
    void UpdateMCPStatus() {
        if (!m_mcpStatusLabel) return;
        
        if (m_mcpCheckbox && m_mcpCheckbox->GetValue()) {
            m_mcpStatusLabel->SetLabel(wxT("\u2713 Workspace: ") + wxString(m_fsProvider->getRootPath()));
            m_mcpStatusLabel->SetForegroundColour(wxColour(100, 180, 100));
        } else {
            m_mcpStatusLabel->SetLabel("");
        }
        m_panel->Layout();
    }
    
    void LoadConfig() {
        AI::GeminiClient::Instance().LoadFromConfig();
        
        // Set provider choice
        AI::AIProvider provider = AI::GeminiClient::Instance().GetProvider();
        m_providerChoice->SetSelection(provider == AI::AIProvider::Cortex ? 1 : 0);
        
        // Populate model list for current provider
        PopulateModelList();
    }
    
    void UpdateApiKeyWarning() {
        bool hasKey = AI::GeminiClient::Instance().HasApiKey();
        AI::AIProvider provider = AI::GeminiClient::Instance().GetProvider();
        
        // For Cortex, also check if base URL is set
        bool needsUrl = (provider == AI::AIProvider::Cortex && 
                        AI::GeminiClient::Instance().GetBaseUrl().empty());
        
        if (needsUrl) {
            m_apiKeyWarning->SetLabel(wxT("\u26A0 Set ai.baseUrl and ai.apiKey in config")); // ⚠
            m_apiKeyWarning->Show(true);
            m_sendBtn->Enable(false);
        } else if (!hasKey) {
            m_apiKeyWarning->SetLabel(wxT("\u26A0 Set ai.apiKey in config")); // ⚠
            m_apiKeyWarning->Show(true);
            m_sendBtn->Enable(false);
        } else {
            m_apiKeyWarning->Show(false);
            m_sendBtn->Enable(true);
        }
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
#ifdef __WXMSW__
        UpdateStatus(wxT("[...] Thinking..."));
#else
        UpdateStatus(wxT("\U0001F504 Thinking...")); // 🔄
#endif
        m_sendBtn->Enable(false);
        
        // Send message in background thread
        std::string msgStr = message.ToStdString();
        std::thread([this, msgStr]() {
            ProcessMessageWithMCP(msgStr);
        }).detach();
        
        // Start timer to check for responses
        m_responseTimer.Start(100);
    }
    
    /**
     * Process a message, handling MCP tool calls if needed.
     * This runs in a background thread.
     */
    void ProcessMessageWithMCP(const std::string& message) {
        AI::GeminiResponse response = AI::GeminiClient::Instance().SendMessage(message);
        
        // Handle tool calls in a loop (up to max iterations)
        int toolCallCount = 0;
        const int maxToolCalls = 5;
        
        while (response.needsFunctionCall() && toolCallCount < maxToolCalls) {
            toolCallCount++;
            
            // Notify UI about tool call
            {
                std::lock_guard<std::mutex> lock(m_responseMutex);
                PendingResponse toolNotify;
                toolNotify.text = wxT("\U0001F527 Using tool: ") + wxString(response.functionName); // 🔧
                toolNotify.isError = false;
                toolNotify.isToolCall = true;
                toolNotify.toolName = wxString(response.functionName);
                toolNotify.toolArgs = wxString(response.functionArgs);
                m_pendingResponses.push(toolNotify);
            }
            
            // Parse arguments and execute tool
            MCP::Value args = ParseJsonArgs(response.functionArgs);
            MCP::ToolResult toolResult = MCP::Registry::Instance().executeTool(
                response.functionName, args);
            
            // Format tool result
            std::string resultStr;
            if (toolResult.success) {
                resultStr = toolResult.result.toJson();
            } else {
                resultStr = "{\"error\": \"" + toolResult.error + "\"}";
            }
            
            // Continue conversation with tool result
            response = AI::GeminiClient::Instance().ContinueWithToolResult(
                response.functionName, resultStr);
        }
        
        // Queue final response for UI thread
        {
            std::lock_guard<std::mutex> lock(m_responseMutex);
            if (response.isOk() && !response.hasFunctionCall) {
                m_pendingResponses.push({wxString(response.text), false, false, "", ""});
            } else if (response.hasFunctionCall) {
                m_pendingResponses.push({
                    wxString("Reached maximum tool calls. Last response may be incomplete."), 
                    true, false, "", ""});
            } else {
                m_pendingResponses.push({wxString(response.error), true, false, "", ""});
            }
        }
        
        m_isLoading = false;
    }
    
    /**
     * Parse JSON arguments string into MCP::Value.
     * Simple recursive parser for the arguments object.
     */
    MCP::Value ParseJsonArgs(const std::string& json) {
        MCP::Value result;
        if (json.empty() || json[0] != '{') return result;
        
        // Very basic JSON object parser
        size_t pos = 1;
        while (pos < json.size()) {
            // Skip whitespace
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
                pos++;
            }
            
            if (pos >= json.size() || json[pos] == '}') break;
            
            // Skip comma
            if (json[pos] == ',') {
                pos++;
                continue;
            }
            
            // Parse key
            if (json[pos] != '"') break;
            pos++;
            size_t keyStart = pos;
            while (pos < json.size() && json[pos] != '"') pos++;
            std::string key = json.substr(keyStart, pos - keyStart);
            pos++; // skip closing quote
            
            // Skip colon
            while (pos < json.size() && json[pos] != ':') pos++;
            pos++;
            
            // Skip whitespace
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
                pos++;
            }
            
            // Parse value
            if (pos >= json.size()) break;
            
            if (json[pos] == '"') {
                // String value
                pos++;
                size_t valStart = pos;
                while (pos < json.size() && json[pos] != '"') {
                    if (json[pos] == '\\' && pos + 1 < json.size()) pos++; // skip escaped char
                    pos++;
                }
                std::string val = json.substr(valStart, pos - valStart);
                // Unescape basic sequences
                std::string unescaped;
                for (size_t i = 0; i < val.size(); i++) {
                    if (val[i] == '\\' && i + 1 < val.size()) {
                        if (val[i+1] == 'n') { unescaped += '\n'; i++; }
                        else if (val[i+1] == 't') { unescaped += '\t'; i++; }
                        else if (val[i+1] == '"') { unescaped += '"'; i++; }
                        else if (val[i+1] == '\\') { unescaped += '\\'; i++; }
                        else unescaped += val[i];
                    } else {
                        unescaped += val[i];
                    }
                }
                result[key] = unescaped;
                pos++;
            } else if (json[pos] == 't' || json[pos] == 'f') {
                // Boolean value
                bool val = (json[pos] == 't');
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
                result[key] = val;
            } else if (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')) {
                // Number value
                size_t numStart = pos;
                while (pos < json.size() && (json[pos] == '-' || json[pos] == '.' || 
                       (json[pos] >= '0' && json[pos] <= '9'))) {
                    pos++;
                }
                double val = std::stod(json.substr(numStart, pos - numStart));
                result[key] = val;
            } else {
                // Skip unknown value types
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
            }
        }
        
        return result;
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
            if (response.isToolCall) {
                // Show tool call notification as a system message
                AddToolCallBubble(response.toolName, response.toolArgs);
#ifdef __WXMSW__
                UpdateStatus(wxT("[...] Executing tool..."));
#else
                UpdateStatus(wxT("\U0001F504 Executing tool...")); // 🔄
#endif
            } else {
                AddMessageBubble(response.text, false, response.isError);
                UpdateStatus("");
                m_sendBtn->Enable(AI::GeminiClient::Instance().HasApiKey());
            }
            
            if (m_pendingResponses.empty() && !m_isLoading) {
                m_responseTimer.Stop();
            }
        } else if (!m_isLoading) {
            m_responseTimer.Stop();
            UpdateStatus("");
            m_sendBtn->Enable(AI::GeminiClient::Instance().HasApiKey());
        }
    }
    
    /**
     * Add a tool call notification bubble.
     */
    void AddToolCallBubble(const wxString& toolName, const wxString& args) {
        wxString text = wxT("\U0001F527 Tool: ") + toolName; // 🔧
        
        // Add truncated args preview
        if (!args.IsEmpty()) {
            wxString preview = args.Left(100);
            if (args.Length() > 100) preview += "...";
            text += "\n" + preview;
        }
        
        // Create a special bubble with different styling
        auto* bubble = new ChatMessageBubble(m_chatPanel, text, false, false);
        bubble->SetThemeColors(m_bgColor, wxColour(180, 180, 220)); // Light purple for tool calls
        m_chatSizer->Add(bubble, 0, wxEXPAND | wxALL, 5);
        m_chatPanel->Layout();
        m_chatPanel->FitInside();
        m_chatPanel->Scroll(-1, m_chatPanel->GetVirtualSize().GetHeight());
    }
    
    void OnClearChat(wxCommandEvent& event) {
        ClearConversation();
    }
    
    void OnModelChanged(wxCommandEvent& event) {
        wxString model = m_modelChoice->GetStringSelection();
        AI::GeminiClient::Instance().SetModel(model.ToStdString());
        AI::GeminiClient::Instance().SaveToConfig();
    }
    
    void OnProviderChanged(wxCommandEvent& event) {
        int sel = m_providerChoice->GetSelection();
        AI::AIProvider provider = (sel == 1) ? AI::AIProvider::Cortex : AI::AIProvider::Gemini;
        AI::GeminiClient::Instance().SetProvider(provider);
        
        // Update model list for new provider
        PopulateModelList();
        
        // Clear conversation when switching providers
        ClearConversation();
        
        // Update API key warning
        UpdateApiKeyWarning();
        
        AI::GeminiClient::Instance().SaveToConfig();
        
        // Show helpful message about configuration
        if (provider == AI::AIProvider::Cortex) {
            AddMessageBubble(
                "Switched to Cortex provider. Make sure to set:\n"
                "- ai.baseUrl: Your Cortex endpoint URL\n"
                "- ai.apiKey: Your API key\n\n"
                "These can be configured in ~/.bytemusehq/config.json", 
                false);
        } else {
            AddMessageBubble(
                "Switched to Gemini provider. Make sure ai.apiKey is set "
                "to your Google AI API key in config.", 
                false);
        }
    }
    
    void PopulateModelList(bool fetchFromApi = false) {
        if (!m_modelChoice) return;
        
        m_modelChoice->Clear();
        
        std::vector<std::string> models;
        if (fetchFromApi) {
            UpdateStatus("Fetching models...");
            models = AI::GeminiClient::Instance().FetchAvailableModels();
            UpdateStatus("");
        } else {
            // Use fallback list initially (faster startup)
            models = AI::GeminiClient::GetFallbackModels(
                AI::GeminiClient::Instance().GetProvider());
        }
        
        for (const auto& model : models) {
            m_modelChoice->Append(wxString(model));
        }
        
        // Try to select current model, or first available
        wxString currentModel = wxString(AI::GeminiClient::Instance().GetModel());
        int idx = m_modelChoice->FindString(currentModel);
        if (idx != wxNOT_FOUND) {
            m_modelChoice->SetSelection(idx);
        } else if (m_modelChoice->GetCount() > 0) {
            m_modelChoice->SetSelection(0);
            AI::GeminiClient::Instance().SetModel(m_modelChoice->GetString(0).ToStdString());
        }
    }
    
    void RefreshModelList() {
        PopulateModelList(true);
    }
    
    void OnRefreshModels(wxCommandEvent& event) {
        RefreshModelList();
    }
    
    void OnMCPToggle(wxCommandEvent& event) {
        bool enabled = m_mcpCheckbox->GetValue();
        AI::GeminiClient::Instance().SetMCPEnabled(enabled);
        
        if (m_fsProvider) {
            m_fsProvider->setEnabled(enabled);
        }
        
        UpdateMCPStatus();
    }
    
    void OnChatPanelResize(wxSizeEvent& event) {
        // When the chat panel resizes (e.g., from splitter drag), 
        // trigger layout to let bubbles recalculate their heights
        if (m_chatPanel) {
            m_chatPanel->Layout();
            m_chatPanel->FitInside();
        }
        event.Skip();
    }
};

} // namespace BuiltinWidgets

#endif // GEMINI_CHAT_WIDGET_H
