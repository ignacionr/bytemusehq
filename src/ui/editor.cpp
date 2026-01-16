#include "editor.h"
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

wxBEGIN_EVENT_TABLE(Editor, wxPanel)
    EVT_STC_SAVEPOINTREACHED(wxID_ANY, Editor::OnSavePointReached)
    EVT_STC_SAVEPOINTLEFT(wxID_ANY, Editor::OnSavePointLeft)
wxEND_EVENT_TABLE()

Editor::Editor(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id)
    , m_textCtrl(nullptr)
    , m_isModified(false)
{
    SetupTextCtrl();
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
    m_textCtrl->SetCaretLineBackground(wxColour(245, 245, 245));
    
    // Word wrap (off by default)
    m_textCtrl->SetWrapMode(wxSTC_WRAP_NONE);
    
    // Default to no lexer
    m_textCtrl->SetLexer(wxSTC_LEX_NULL);
    
    sizer->Add(m_textCtrl, 1, wxEXPAND);
    SetSizer(sizer);
}

void Editor::ConfigureLexer(const wxString& extension)
{
    wxString ext = extension.Lower();
    
    // Reset styles first
    m_textCtrl->StyleClearAll();
    
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
        
        // Styling for C++
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENT, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTDOC, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_NUMBER, wxColour(0, 0, 192));
        m_textCtrl->StyleSetForeground(wxSTC_C_WORD, wxColour(0, 0, 255));
        m_textCtrl->StyleSetBold(wxSTC_C_WORD, true);
        m_textCtrl->StyleSetForeground(wxSTC_C_STRING, wxColour(163, 21, 21));
        m_textCtrl->StyleSetForeground(wxSTC_C_CHARACTER, wxColour(163, 21, 21));
        m_textCtrl->StyleSetForeground(wxSTC_C_PREPROCESSOR, wxColour(128, 0, 128));
        m_textCtrl->StyleSetForeground(wxSTC_C_OPERATOR, wxColour(0, 0, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_IDENTIFIER, wxColour(0, 0, 0));
    }
    else if (ext == "py") {
        m_textCtrl->SetLexer(wxSTC_LEX_PYTHON);
        m_textCtrl->SetKeyWords(0,
            "and as assert async await break class continue def del elif else "
            "except finally for from global if import in is lambda nonlocal not "
            "or pass raise return try while with yield None True False");
        
        m_textCtrl->StyleSetForeground(wxSTC_P_COMMENTLINE, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_P_NUMBER, wxColour(0, 0, 192));
        m_textCtrl->StyleSetForeground(wxSTC_P_STRING, wxColour(163, 21, 21));
        m_textCtrl->StyleSetForeground(wxSTC_P_WORD, wxColour(0, 0, 255));
        m_textCtrl->StyleSetBold(wxSTC_P_WORD, true);
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
        
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENT, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_COMMENTLINE, wxColour(0, 128, 0));
        m_textCtrl->StyleSetForeground(wxSTC_C_NUMBER, wxColour(0, 0, 192));
        m_textCtrl->StyleSetForeground(wxSTC_C_WORD, wxColour(0, 0, 255));
        m_textCtrl->StyleSetBold(wxSTC_C_WORD, true);
        m_textCtrl->StyleSetForeground(wxSTC_C_STRING, wxColour(163, 21, 21));
    }
    else if (ext == "json") {
        m_textCtrl->SetLexer(wxSTC_LEX_JSON);
        m_textCtrl->StyleSetForeground(wxSTC_JSON_STRING, wxColour(163, 21, 21));
        m_textCtrl->StyleSetForeground(wxSTC_JSON_NUMBER, wxColour(0, 0, 192));
        m_textCtrl->StyleSetForeground(wxSTC_JSON_PROPERTYNAME, wxColour(0, 0, 255));
    }
    else if (ext == "xml" || ext == "html" || ext == "htm") {
        m_textCtrl->SetLexer(wxSTC_LEX_HTML);
        m_textCtrl->StyleSetForeground(wxSTC_H_TAG, wxColour(0, 0, 255));
        m_textCtrl->StyleSetForeground(wxSTC_H_ATTRIBUTE, wxColour(255, 0, 0));
        m_textCtrl->StyleSetForeground(wxSTC_H_DOUBLESTRING, wxColour(163, 21, 21));
        m_textCtrl->StyleSetForeground(wxSTC_H_SINGLESTRING, wxColour(163, 21, 21));
    }
    else if (ext == "md" || ext == "markdown") {
        m_textCtrl->SetLexer(wxSTC_LEX_MARKDOWN);
    }
    else if (ext == "cmake" || ext == "txt" && m_currentFilePath.Lower().Contains("cmakelists")) {
        m_textCtrl->SetLexer(wxSTC_LEX_CMAKE);
    }
    else {
        // Plain text
        m_textCtrl->SetLexer(wxSTC_LEX_NULL);
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
    SetModified(false);
    
    // Configure lexer based on file extension
    wxFileName fileName(path);
    ConfigureLexer(fileName.GetExt());
    
    NotifyFileChanged();
    
    return true;
}

bool Editor::Save()
{
    if (m_currentFilePath.IsEmpty()) {
        return SaveAs();
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
