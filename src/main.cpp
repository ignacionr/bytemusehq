#include <wx/app.h>
#include "ui/frame.h"

class ByteMuseApp : public wxApp {
public:
    bool OnInit() override {
        auto frame = new MainFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(ByteMuseApp);
