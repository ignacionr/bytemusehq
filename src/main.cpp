#include <wx/app.h>
#include "ui/frame.h"
#include "config/config.h"
#include "theme/theme.h"

class ByteMuseApp : public wxApp {
public:
    bool OnInit() override {
        // Initialize configuration system
        Config::Instance().Load();
        
        // Initialize theme system
        ThemeManager::Instance().Initialize();
        
        auto frame = new MainFrame();
        frame->Show();
        return true;
    }
    
    int OnExit() override {
        // Save configuration on exit
        Config::Instance().Save();
        return wxApp::OnExit();
    }
};

wxIMPLEMENT_APP(ByteMuseApp);
