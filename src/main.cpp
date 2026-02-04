#include <wx/app.h>
#include <wx/log.h>
#include <wx/image.h>
#include "ui/frame.h"
#include "config/config.h"
#include "theme/theme.h"

class ByteMuseApp : public wxApp {
public:
    bool OnInit() override {
        // Suppress the default wxLogGui popup dialogs IMMEDIATELY
        // This prevents any early logs from showing as popups
        wxLog::EnableLogging(false);
        
        // Initialize image handlers for loading PNG, JPEG, etc.
        wxInitAllImageHandlers();
        
        // Initialize configuration system
        Config::Instance().Load();
        
        // Initialize theme system
        ThemeManager::Instance().Initialize();
        
        auto frame = new MainFrame();
        
        // Create log window - all logs go here, not to popups
        // Parameters: parent, title, show, passToOld
        // passToOld=false prevents logs from going to default wxLogGui (which shows popups)
        m_logWindow = new wxLogWindow(frame, "Debug Log", false, false);
        
        // Now that log window is set up, re-enable logging
        wxLog::EnableLogging(true);
        
        // Enable verbose logging so wxLogDebug messages are shown
        wxLog::SetVerbose(true);
        
        // Set log level to include debug messages
        wxLog::SetLogLevel(wxLOG_Debug);
        
        frame->Show();
        
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
