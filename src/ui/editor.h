#ifndef EDITOR_H
#define EDITOR_H

#include <wx/wx.h>
#include <wx/stc/stc.h>
#include <wx/file.h>
#include <functional>

/**
 * Editor component for ByteMuseHQ.
 * Wraps wxStyledTextCtrl with file management, dirty tracking, and save functionality.
 */
class Editor : public wxPanel {
public:
    // Callback when dirty state changes
    using DirtyStateCallback = std::function<void(bool isDirty)>;
    // Callback when file changes (opened, saved, etc.)
    using FileChangeCallback = std::function<void(const wxString& filePath)>;

    Editor(wxWindow* parent, wxWindowID id = wxID_ANY);
    virtual ~Editor() = default;

    // File operations
    bool OpenFile(const wxString& path);
    bool Save();
    bool SaveAs(const wxString& path);
    bool SaveAs(); // Shows dialog
    void NewFile();

    // State queries
    bool IsModified() const { return m_isModified; }
    bool HasFile() const { return !m_currentFilePath.IsEmpty(); }
    const wxString& GetFilePath() const { return m_currentFilePath; }
    wxString GetFileName() const;
    wxString GetTitle() const; // Returns filename with * if modified

    // Text control access (for commands that need direct access)
    wxStyledTextCtrl* GetTextCtrl() { return m_textCtrl; }
    const wxStyledTextCtrl* GetTextCtrl() const { return m_textCtrl; }

    // Callbacks
    void SetDirtyStateCallback(DirtyStateCallback callback) { m_dirtyCallback = std::move(callback); }
    void SetFileChangeCallback(FileChangeCallback callback) { m_fileChangeCallback = std::move(callback); }

    // Prompt to save if modified. Returns true if it's ok to proceed (saved or discarded)
    bool PromptSaveIfModified();

private:
    wxStyledTextCtrl* m_textCtrl;
    wxString m_currentFilePath;
    bool m_isModified;
    
    // Callbacks
    DirtyStateCallback m_dirtyCallback;
    FileChangeCallback m_fileChangeCallback;

    // Setup methods
    void SetupTextCtrl();
    void ConfigureLexer(const wxString& extension);
    
    // Internal state management
    void SetModified(bool modified);
    void NotifyDirtyStateChanged();
    void NotifyFileChanged();

    // Event handlers
    void OnTextChanged(wxStyledTextEvent& event);
    void OnSavePointReached(wxStyledTextEvent& event);
    void OnSavePointLeft(wxStyledTextEvent& event);

    wxDECLARE_EVENT_TABLE();
};

#endif // EDITOR_H
