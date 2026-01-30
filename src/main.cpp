#include <wx/app.h>
#include <wx/log.h>
#include <wx/image.h>
#include "ui/frame.h"
#include "config/config.h"
#include "theme/theme.h"

class ByteMuseApp : public wxApp {
public:
    bool OnInit() override {
        // Initialize image handlers for loading PNG, JPEG, etc.
        wxInitAllImageHandlers();
        
        // Initialize configuration system
        Config::Instance().Load();
        
        // Initialize theme system
        ThemeManager::Instance().Initialize();
        
        auto frame = new MainFrame();
        frame->Show();
        
        // Create log window (hidden by default)
        // Parameters: parent, title, show, passToOld
        // passToOld=true keeps stderr output as well (useful during development)
        m_logWindow = new wxLogWindow(frame, "Debug Log", false, true);
        
        // Enable verbose logging so wxLogDebug messages are shown
        wxLog::SetVerbose(false);
        
        // Set log level to include debug messages
        wxLog::SetLogLevel(wxLOG_Warning);
        
        return true;
    }
    
    int OnExit() override {
        // Save configuration on exit
        Config::Instance().Save();
        return wxApp::OnExit();
    }
    
    // Toggle visibility of the log window
    void ToggleLogWindow() {
        if (m_logWindow) {
            wxFrame* logFrame = m_logWindow->GetFrame();
            if (logFrame) {
                logFrame->Show(!logFrame->IsShown());
                if (logFrame->IsShown()) {
                    logFrame->Raise();
                }
            }
        }
    }
    
    bool IsLogWindowVisible() const {
        if (m_logWindow) {
            wxFrame* logFrame = m_logWindow->GetFrame();
            return logFrame && logFrame->IsShown();
        }
        return false;
    }
    
private:
    wxLogWindow* m_logWindow = nullptr;
};

wxIMPLEMENT_APP(ByteMuseApp);
