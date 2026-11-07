#include <gtest/gtest.h>
#include <wx/app.h>
#include <wx/init.h>

#include "model/Language.h"
#include "ui/Catalogue.h"
#include "ui/Localise.h"

#include <cstdlib>

namespace {
class TestApp : public wxApp {
public:
    bool OnInit() override { return true; }
    int OnRun() override { return 0; }
    wxLayoutDirection GetLayoutDirection() const override { return language_layout_direction(); }
};
}  // namespace

// The wx tests need an application object and a display; gtest_main has neither.
int main(int argc, char** argv) {
    // ctest sets this too; a direct run gets X11 (the GTK default under Wayland is not what the
    // tests drive)
    setenv("GDK_BACKEND", "x11", 0);
    ::testing::InitGoogleTest(&argc, argv);
    // The build lists the tests (gtest_discover_tests) where there may be no display.
    if (GTEST_FLAG_GET(list_tests))
        return RUN_ALL_TESTS();
    wxApp::SetInstance(new TestApp);
    if (!wxEntryStart(argc, argv))
        return 1;
    wxTheApp->CallOnInit();
    register_indicator_strings();
    install_wx_catalogues();
    int const result = RUN_ALL_TESTS();
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
