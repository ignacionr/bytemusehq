#include "editor.h"
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <cstdio>
#include <string>

#ifdef _WIN32
#include <io.h>
#endif

wxBEGIN_EVENT_TABLE(Editor, wxPanel)
    EVT_STC_SAVEPOINTREACHED(wxID_ANY, Editor::OnSavePointReached)
    EVT_STC_SAVEPOINTLEFT(wxID_ANY, Editor::OnSavePointLeft)
wxEND_EVENT_TABLE()

Editor::Editor(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
    , m_textCtrl(nullptr)
    , m_isModified(false)
    , m_themeListenerId(0)
{
    SetupTextCtrl();
    ApplyCurrentTheme();
    
    // Listen for theme changes
    m_themeListenerId = ThemeManager::Instance().AddChangeListener(
        [this](const ThemePtr& theme) {
            ApplyTheme(theme);
        });
}

void Editor::SetupTextCtrl()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    
    m_textCtrl = new wxStyledTextCtrl(this, wxID_ANY);
    
    // Basic editor configuration
    m_textCtrl->StyleClearAll();
    
    // Line numbers margin
    m_textCtrl->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    m_textCtrl->SetMarginWidth(0, 50);
    
    // Folding margin (optional, can be expanded later)
    m_textCtrl->SetMarginType(1, wxSTC_MARGIN_SYMBOL);
    m_textCtrl->SetMarginWidth(1, 0); // Disabled for now
    
    // Tab settings
    m_textCtrl->SetTabWidth(4);
    m_textCtrl->SetUseTabs(false);
    m_textCtrl->SetIndent(4);
    m_textCtrl->SetTabIndents(true);
    m_textCtrl->SetBackSpaceUnIndents(true);
    
    // Auto-indent
    m_textCtrl->SetIndentationGuides(wxSTC_IV_LOOKBOTH);
    
    // Caret and selection
    m_textCtrl->SetCaretLineVisible(true);
    
    // Word wrap (off by default)
    m_textCtrl->SetWrapMode(wxSTC_WRAP_NONE);
    
    // Default to no lexer
    m_textCtrl->SetLexer(wxSTC_LEX_NULL);
    
    sizer->Add(m_textCtrl, 1, wxEXPAND);
    SetSizer(sizer);
}

void Editor::ApplyCurrentTheme()
{
    auto theme = ThemeManager::Instance().GetCurrentTheme();
    if (theme) {
        ApplyTheme(theme);
    }
}

void Editor::ApplyTheme(const ThemePtr& theme)
{
    if (!theme || !m_textCtrl) return;
    
    const auto& colors = theme->editor;
    
    // Set all styles to default first
    for (int i = 0; i < wxSTC_STYLE_MAX; i++) {
        m_textCtrl->StyleSetBackground(i, colors.background);
        m_textCtrl->StyleSetForeground(i, colors.foreground);
    }
    
    // Editor background and foreground
    m_textCtrl->StyleSetBackground(wxSTC_STYLE_DEFAULT, colors.background);
    m_textCtrl->StyleSetForeground(wxSTC_STYLE_DEFAULT, colors.foreground);
    m_textCtrl->StyleClearAll();  // Apply default to all styles
    
    // Line numbers
    m_textCtrl->StyleSetBackground(wxSTC_STYLE_LINENUMBER, colors.lineNumberBackground);
    m_textCtrl->StyleSetForeground(wxSTC_STYLE_LINENUMBER, colors.lineNumberForeground);
    m_textCtrl->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    m_textCtrl->SetMarginWidth(0, 50);
    
    // Caret and caret line
    m_textCtrl->SetCaretForeground(colors.caret);
    m_textCtrl->SetCaretLineVisible(true);
    m_textCtrl->SetCaretLineBackground(colors.caretLine);
    
    // Selection
    m_textCtrl->SetSelBackground(true, colors.selection);
    m_textCtrl->SetSelForeground(true, colors.selectionForeground);
    
    // Whitespace
    m_textCtrl->SetWhitespaceForeground(true, colors.whitespace);
    
    // Indent guides
    m_textCtrl->StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, colors.indentGuide);
    
    // Re-apply syntax highlighting for current lexer
    ApplySyntaxColors(theme);
    
    // Refresh display
    m_textCtrl->Refresh();
}

void Editor::ApplySyntaxColors(const ThemePtr& theme)
{
    if (!theme || !m_textCtrl) return;
    
    const auto& colors = theme->editor;
    int lexer = m_textCtrl->GetLexer();
    
    // Apply colors based on current lexer
    if (lexer == wxSTC_LEX_CPP) {
        // C/C++ styling
        m_textCtrl->StyleSetForeground(wxSTC_C_DEFAULT, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENT, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINE, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTDOC, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINEDOC, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_C_NUMBER, colors.number);
        m_textCtrl->StyleSetForeground(wxSTC_C_WORD, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_C_WORD, true);
        m_textCtrl->StyleSetForeground(wxSTC_C_STRING, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_C_CHARACTER, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_C_PREPROCESSOR, colors.preprocessor);
        m_textCtrl->StyleSetForeground(wxSTC_C_OPERATOR, colors.operator_);
        m_textCtrl->StyleSetForeground(wxSTC_C_IDENTIFIER, colors.identifier);
        m_textCtrl->StyleSetForeground(wxSTC_C_STRINGEOL, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_C_VERBATIM, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_C_REGEX, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_C_WORD2, colors.type);
        
        // Background for all C++ styles
        for (int i = wxSTC_C_DEFAULT; i <= wxSTC_C_PREPROCESSORCOMMENTDOC; i++) {
            m_textCtrl->StyleSetBackground(i, colors.background);
        }
    }
    else if (lexer == wxSTC_LEX_PYTHON) {
        m_textCtrl->StyleSetForeground(wxSTC_P_DEFAULT, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_P_COMMENTLINE, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_P_COMMENTBLOCK, colors.comment);
        m_textCtrl->StyleSetForeground(wxSTC_P_NUMBER, colors.number);
        m_textCtrl->StyleSetForeground(wxSTC_P_STRING, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_P_CHARACTER, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_P_WORD, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_P_WORD, true);
        m_textCtrl->StyleSetForeground(wxSTC_P_TRIPLE, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_P_TRIPLEDOUBLE, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_P_CLASSNAME, colors.type);
        m_textCtrl->StyleSetForeground(wxSTC_P_DEFNAME, colors.function);
        m_textCtrl->StyleSetForeground(wxSTC_P_OPERATOR, colors.operator_);
        m_textCtrl->StyleSetForeground(wxSTC_P_IDENTIFIER, colors.identifier);
        m_textCtrl->StyleSetForeground(wxSTC_P_DECORATOR, colors.preprocessor);
        
        // Background for all Python styles
        for (int i = wxSTC_P_DEFAULT; i <= wxSTC_P_DECORATOR; i++) {
            m_textCtrl->StyleSetBackground(i, colors.background);
        }
    }
    else if (lexer == wxSTC_LEX_JSON) {
        m_textCtrl->StyleSetForeground(wxSTC_JSON_DEFAULT, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_STRING, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_NUMBER, colors.number);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_PROPERTYNAME, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_KEYWORD, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_OPERATOR, colors.operator_);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_ERROR, wxColour(255, 0, 0));
        
        for (int i = wxSTC_JSON_DEFAULT; i <= wxSTC_JSON_ERROR; i++) {
            m_textCtrl->StyleSetBackground(i, colors.background);
        }
    }
    else if (lexer == wxSTC_LEX_HTML) {
        m_textCtrl->StyleSetForeground(wxSTC_H_DEFAULT, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_H_TAG, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_H_TAGUNKNOWN, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_H_ATTRIBUTE, colors.type);
        m_textCtrl->StyleSetForeground(wxSTC_H_ATTRIBUTEUNKNOWN, colors.type);
        m_textCtrl->StyleSetForeground(wxSTC_H_NUMBER, colors.number);
        m_textCtrl->StyleSetForeground(wxSTC_H_DOUBLESTRING, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_H_SINGLESTRING, colors.string);
        m_textCtrl->StyleSetForeground(wxSTC_H_COMMENT, colors.comment);
        
        for (int i = wxSTC_H_DEFAULT; i <= wxSTC_H_QUESTION; i++) {
            m_textCtrl->StyleSetBackground(i, colors.background);
        }
    }
    else if (lexer == wxSTC_LEX_MARKDOWN) {
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_DEFAULT, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_LINE_BEGIN, colors.foreground);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_STRONG1, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_MARKDOWN_STRONG1, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_STRONG2, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_MARKDOWN_STRONG2, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_EM1, colors.string);
        m_textCtrl->StyleSetItalic(wxSTC_MARKDOWN_EM1, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_EM2, colors.string);
        m_textCtrl->StyleSetItalic(wxSTC_MARKDOWN_EM2, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER1, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_MARKDOWN_HEADER1, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER2, colors.keyword);
        m_textCtrl->StyleSetBold(wxSTC_MARKDOWN_HEADER2, true);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER3, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER4, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER5, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_HEADER6, colors.keyword);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_CODE, colors.function);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_CODE2, colors.function);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_CODEBK, colors.function);
        m_textCtrl->StyleSetForeground(wxSTC_MARKDOWN_LINK, colors.identifier);
        
        for (int i = wxSTC_MARKDOWN_DEFAULT; i <= wxSTC_MARKDOWN_CODEBK; i++) {
            m_textCtrl->StyleSetBackground(i, colors.background);
        }
    }
}

void Editor::ConfigureLexer(const wxString& extension)
{
    wxString ext = extension.Lower();
    
    // Get current theme colors
    auto theme = ThemeManager::Instance().GetCurrentTheme();
    
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || ext == "cc" || ext == "cxx") {
        m_textCtrl->SetLexer(wxSTC_LEX_CPP);
        
        // C++ keywords
        m_textCtrl->SetKeyWords(0, 
            "alignas alignof and and_eq asm auto bitand bitor bool break case catch "
            "char char8_t char16_t char32_t class compl concept const consteval "
            "constexpr constinit const_cast continue co_await co_return co_yield "
            "decltype default delete do double dynamic_cast else enum explicit "
            "export extern false float for friend goto if inline int long mutable "
            "namespace new noexcept not not_eq nullptr operator or or_eq private "
            "protected public register reinterpret_cast requires return short signed "
            "sizeof static static_assert static_cast struct switch template this "
            "thread_local throw true try typedef typeid typename union unsigned "
            "using virtual void volatile wchar_t while xor xor_eq "
            "override final");
    }
    else if (ext == "py") {
        m_textCtrl->SetLexer(wxSTC_LEX_PYTHON);
        m_textCtrl->SetKeyWords(0,
            "and as assert async await break class continue def del elif else "
            "except finally for from global if import in is lambda nonlocal not "
            "or pass raise return try while with yield None True False");
    }
    else if (ext == "js" || ext == "ts" || ext == "jsx" || ext == "tsx") {
        m_textCtrl->SetLexer(wxSTC_LEX_CPP); // JavaScript uses CPP lexer
        m_textCtrl->SetKeyWords(0,
            "abstract arguments await boolean break byte case catch char class "
            "const continue debugger default delete do double else enum export "
            "extends false final finally float for function goto if implements "
            "import in instanceof int interface let long native new null package "
            "private protected public return short static super switch synchronized "
            "this throw throws transient true try typeof var void volatile while with yield");
    }
    else if (ext == "json") {
        m_textCtrl->SetLexer(wxSTC_LEX_JSON);
    }
    else if (ext == "xml" || ext == "html" || ext == "htm") {
        m_textCtrl->SetLexer(wxSTC_LEX_HTML);
    }
    else if (ext == "md" || ext == "markdown") {
        m_textCtrl->SetLexer(wxSTC_LEX_MARKDOWN);
    }
    else if (ext == "cmake" || (ext == "txt" && m_currentFilePath.Lower().Contains("cmakelists"))) {
        m_textCtrl->SetLexer(wxSTC_LEX_CMAKE);
    }
    else {
        // Plain text
        m_textCtrl->SetLexer(wxSTC_LEX_NULL);
    }
    
    // Apply theme colors for the lexer
    if (theme) {
        ApplySyntaxColors(theme);
    }
}

bool Editor::OpenFile(const wxString& path)
{
    // Check for unsaved changes
    if (!PromptSaveIfModified()) {
        return false;
    }
    
    wxFile file(path);
    if (!file.IsOpened()) {
        wxMessageBox("Could not open file: " + path, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    wxString content;
    if (!file.ReadAll(&content)) {
        wxMessageBox("Could not read file: " + path, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    m_textCtrl->SetText(content);
    m_textCtrl->EmptyUndoBuffer();
    m_textCtrl->SetSavePoint();
    
    m_currentFilePath = path;
    m_isRemoteFile = false;
    m_sshPrefix.clear();
    SetModified(false);
    
    // Configure lexer based on file extension
    wxFileName fileName(path);
    ConfigureLexer(fileName.GetExt());
    
    NotifyFileChanged();
    
    return true;
}

bool Editor::OpenRemoteFile(const wxString& remotePath, const std::string& sshPrefix)
{
    wxLogMessage("Editor::OpenRemoteFile: remotePath='%s'", remotePath);
    wxLogMessage("Editor::OpenRemoteFile: sshPrefix='%s'", sshPrefix.c_str());
    
    // Check for unsaved changes
    if (!PromptSaveIfModified()) {
        return false;
    }
    
    // Fetch file content via SSH
    std::string cmd = sshPrefix + " \"cat \\\"" + remotePath.ToStdString() + "\\\"\" 2>&1";
    wxLogMessage("Editor::OpenRemoteFile: command='%s'", cmd.c_str());
    
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    
    if (!pipe) {
        wxLogError("Could not connect to remote host");
        return false;
    }
    
    std::string content;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        content += buffer;
    }
    
#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif
    
    if (status != 0) {
        wxLogError("Could not read remote file: %s (exit code: %d)", remotePath, status);
        return false;
    }
    
    m_textCtrl->SetText(wxString(content));
    m_textCtrl->EmptyUndoBuffer();
    m_textCtrl->SetSavePoint();
    
    m_currentFilePath = remotePath;
    m_isRemoteFile = true;
    m_sshPrefix = sshPrefix;
    SetModified(false);
    
    // Configure lexer based on file extension
    wxFileName fileName(remotePath);
    ConfigureLexer(fileName.GetExt());
    
    NotifyFileChanged();
    
    return true;
}

bool Editor::Save()
{
    if (m_currentFilePath.IsEmpty()) {
        return SaveAs();
    }
    
    // Handle remote file save
    if (m_isRemoteFile && !m_sshPrefix.empty()) {
        wxString content = m_textCtrl->GetText();
        
        // Write content to remote file via SSH
        // Use a temp file approach for reliability
        wxString tempPath = wxFileName::CreateTempFileName("bytemuse_");
        wxFile tempFile(tempPath, wxFile::write);
        if (!tempFile.IsOpened() || !tempFile.Write(content)) {
            wxMessageBox("Could not create temp file for remote save", "Error", wxOK | wxICON_ERROR, this);
            return false;
        }
        tempFile.Close();
        
        // Use scp to copy file
        std::string scpCmd = "scp";
        // Extract host from ssh prefix (after last space)
        size_t lastSpace = m_sshPrefix.rfind(' ');
        std::string hostPart = lastSpace != std::string::npos ? m_sshPrefix.substr(lastSpace + 1) : "";
        
        scpCmd += " \"" + tempPath.ToStdString() + "\" " + hostPart + ":\"" + m_currentFilePath.ToStdString() + "\"";
        
        int result = system(scpCmd.c_str());
        wxRemoveFile(tempPath);
        
        if (result != 0) {
            wxMessageBox("Could not save remote file: " + m_currentFilePath, "Error", wxOK | wxICON_ERROR, this);
            return false;
        }
        
        m_textCtrl->SetSavePoint();
        SetModified(false);
        return true;
    }
    
    wxFile file(m_currentFilePath, wxFile::write);
    if (!file.IsOpened()) {
        wxMessageBox("Could not save file: " + m_currentFilePath, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    wxString content = m_textCtrl->GetText();
    if (!file.Write(content)) {
        wxMessageBox("Error writing to file: " + m_currentFilePath, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    m_textCtrl->SetSavePoint();
    SetModified(false);
    
    return true;
}

bool Editor::SaveAs(const wxString& path)
{
    wxFile file(path, wxFile::write);
    if (!file.IsOpened()) {
        wxMessageBox("Could not save file: " + path, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    wxString content = m_textCtrl->GetText();
    if (!file.Write(content)) {
        wxMessageBox("Error writing to file: " + path, "Error", wxOK | wxICON_ERROR, this);
        return false;
    }
    
    m_currentFilePath = path;
    m_textCtrl->SetSavePoint();
    SetModified(false);
    
    // Reconfigure lexer for new extension
    wxFileName fileName(path);
    ConfigureLexer(fileName.GetExt());
    
    NotifyFileChanged();
    
    return true;
}

bool Editor::SaveAs()
{
    wxFileDialog dlg(this, "Save File As", "", GetFileName(),
                     "All files (*.*)|*.*|"
                     "C++ files (*.cpp;*.h;*.hpp)|*.cpp;*.h;*.hpp|"
                     "Text files (*.txt)|*.txt",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    
    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }
    
    return SaveAs(dlg.GetPath());
}

void Editor::NewFile()
{
    if (!PromptSaveIfModified()) {
        return;
    }
    
    m_textCtrl->ClearAll();
    m_textCtrl->EmptyUndoBuffer();
    m_textCtrl->SetSavePoint();
    
    m_currentFilePath.Clear();
    SetModified(false);
    
    m_textCtrl->SetLexer(wxSTC_LEX_NULL);
    
    NotifyFileChanged();
}

wxString Editor::GetFileName() const
{
    if (m_currentFilePath.IsEmpty()) {
        return "Untitled";
    }
    wxFileName fileName(m_currentFilePath);
    return fileName.GetFullName();
}

wxString Editor::GetTitle() const
{
    wxString title = GetFileName();
    if (m_isModified) {
        title = "• " + title;
    }
    return title;
}

bool Editor::PromptSaveIfModified()
{
    if (!m_isModified) {
        return true;
    }
    
    wxString message = "Do you want to save changes to " + GetFileName() + "?";
    int result = wxMessageBox(message, "Save Changes", 
                              wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
    
    if (result == wxCANCEL) {
        return false;
    }
    
    if (result == wxYES) {
        return Save();
    }
    
    // User chose No - discard changes
    return true;
}

void Editor::SetModified(bool modified)
{
    if (m_isModified != modified) {
        m_isModified = modified;
        NotifyDirtyStateChanged();
    }
}

void Editor::NotifyDirtyStateChanged()
{
    if (m_dirtyCallback) {
        m_dirtyCallback(m_isModified);
    }
}

void Editor::NotifyFileChanged()
{
    if (m_fileChangeCallback) {
        m_fileChangeCallback(m_currentFilePath);
    }
}

void Editor::OnSavePointReached(wxStyledTextEvent& event)
{
    SetModified(false);
    event.Skip();
}

void Editor::OnSavePointLeft(wxStyledTextEvent& event)
{
    SetModified(true);
    event.Skip();
}
