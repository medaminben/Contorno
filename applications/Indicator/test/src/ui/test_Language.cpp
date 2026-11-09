#include <gtest/gtest.h>

#include <wx/app.h>
#include <wx/bmpbndl.h>
#include <wx/dialog.h>
#include <wx/frame.h>
#include <wx/panel.h>

#include "model/Language.h"
#include "model/Message.h"
#include "ui/FlagIcons.h"
#include "ui/Localise.h"
#include "ui/MessageLog.h"
#include "ui/SegmentDisplay.h"
#include "ui/Text.h"
#include "ui/WxTestApp.h"

namespace test_Language {

namespace {

/// Counts the language-change events it receives.
struct Counter : wxPanel {
    explicit Counter(wxWindow* parent) : wxPanel(parent) {
        Bind(EVT_LANGUAGE_CHANGED, [this](wxCommandEvent& event) {
            ++changes;
            event.Skip();  // as a handler that does not consume it: it would go up to the parent
        });
    }
    int changes {0};
};

}  // namespace

TEST(WxLanguage, ApplyArabicTurnsTheApplicationRightToLeft) {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    apply_language("ar");
    EXPECT_EQ(wxTheApp->GetLayoutDirection(), wxLayout_RightToLeft);
    EXPECT_EQ(frame->GetLayoutDirection(), wxLayout_RightToLeft);
    auto* child = new wxPanel(frame);
    apply_language("ar");
    EXPECT_EQ(child->GetLayoutDirection(), wxLayout_RightToLeft);  // a child follows too
    wxFrame* later = new wxFrame(nullptr, wxID_ANY, "later");  // made after: picks up the default
    EXPECT_EQ(later->GetLayoutDirection(), wxLayout_RightToLeft);
    later->Destroy();
    EXPECT_EQ(current_language(), "ar");
    EXPECT_EQ(text_direction(), TextDirection::right_to_left);
    apply_language("xx");
    EXPECT_EQ(current_language(), "en");
    EXPECT_EQ(wxTheApp->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(frame->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(child->GetLayoutDirection(), wxLayout_LeftToRight);
    apply_language("en");
    frame->Destroy();
}

TEST(WxLanguage, ApplySendsLanguageChange) {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    auto* widget = new Counter(frame);
    apply_language("de");
    EXPECT_EQ(widget->changes, 1);
    apply_language("en");
    EXPECT_EQ(widget->changes, 2);
    frame->Destroy();
}

TEST(WxLanguage, NestedWidgetsGetLanguageChangeOncePerSend) {
    wxFrame* top = new wxFrame(nullptr, wxID_ANY, "t");
    auto* middle = new Counter(top);
    auto* leaf = new Counter(middle);
    apply_language("de");
    EXPECT_EQ(leaf->changes, 1);  // a nested widget is reached, and once, not once per ancestor
    EXPECT_EQ(middle->changes, 1);
    apply_language("en");
    top->Destroy();
}

TEST(WxLanguage, DialogOfAFrameGetsLanguageChangeOnce) {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    auto* dialog = new wxDialog(frame, wxID_ANY, "d");  // top-level and the frame's child
    int changes = 0;
    dialog->Bind(EVT_LANGUAGE_CHANGED, [&changes](wxCommandEvent& e) {
        ++changes;
        e.Skip();
    });
    apply_language("de");
    EXPECT_EQ(changes, 1);
    apply_language("en");
    EXPECT_EQ(changes, 2);
    frame->Destroy();
    dialog = nullptr;
}

// a destroyed frame stays listed until idle: its handlers may belong to an owner that is gone
TEST(WxLanguage, WindowsWaitingForDeletionGetNoLanguageChange) {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    auto* widget = new Counter(frame);
    auto* dialog = new wxDialog(frame, wxID_ANY, "d");
    auto* in_dialog = new Counter(dialog);
    frame->Destroy();
    apply_language("de");
    EXPECT_EQ(widget->changes, 0);
    EXPECT_EQ(in_dialog->changes, 0);
    apply_language("en");
}

TEST(WxLanguage, ApplyRerendersAnExistingLogLine) {
    add_strings("en", {{"t.lang", "Hello"}});
    add_strings("de", {{"t.lang", "Hallo"}});
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    auto* log = new MessageLog(frame);
    apply_language("en");
    log->add(Message("t.lang"));
    EXPECT_TRUE(log->line_text(0).EndsWith("Hello"));
    apply_language("de");  // no rerender() from the owner
    EXPECT_TRUE(log->line_text(0).EndsWith("Hallo"));
    apply_language("en");
    frame->Destroy();
}

TEST(WxLanguage, DisabledFlagIsTheSameAtQtsOpacity) {
    for (std::string const& code : language_codes()) {
        wxImage const on = flag_icon(code).GetBitmap(wxSize(24, 24)).ConvertToImage();
        wxImage const off = flag_icon(code, true).GetBitmap(wxSize(24, 24)).ConvertToImage();
        for (wxPoint const p : {wxPoint(12, 12), wxPoint(12, 3), wxPoint(5, 12)}) {
            EXPECT_NEAR(off.GetAlpha(p.x, p.y), 0.45 * on.GetAlpha(p.x, p.y), 2) << code;
            EXPECT_NEAR(off.GetRed(p.x, p.y), on.GetRed(p.x, p.y), 2) << code;
        }
        EXPECT_GT(off.GetAlpha(12, 12), 0);
    }
}

TEST(WxLanguage, FlagsExistForEveryLanguageAndAreDiscs) {
    for (std::string const& code : language_codes()) {
        wxBitmapBundle const flag = flag_icon(code);
        ASSERT_TRUE(flag.IsOk()) << code;
        wxImage const image = flag.GetBitmap(wxSize(24, 24)).ConvertToImage();
        ASSERT_TRUE(image.HasAlpha()) << code;
        EXPECT_EQ(image.GetAlpha(0, 0), 0) << code;      // the corner is cut
        EXPECT_EQ(image.GetAlpha(12, 12), 255) << code;  // the middle is flag
    }
    // one characteristic pixel per flag, in the Qt flag's colour: swapped flags fail
    struct Sample {
        char const* code;
        int x, y;
        unsigned char r, g, b;
    };
    for (Sample const& s :
         {Sample {"en", 12, 12, 0xC8, 0x10, 0x2E}, Sample {"de", 12, 12, 0xDD, 0x00, 0x00},
          Sample {"es", 12, 12, 0xF1, 0xBF, 0x00}, Sample {"it", 6, 12, 0x00, 0x92, 0x46},
          Sample {"fr", 6, 12, 0x00, 0x26, 0x54}, Sample {"nl", 12, 5, 0xAE, 0x1C, 0x28},
          Sample {"ar", 12, 2, 0xE7, 0x00, 0x13}}) {
        wxImage const image = flag_icon(s.code).GetBitmap(wxSize(24, 24)).ConvertToImage();
        EXPECT_NEAR(image.GetRed(s.x, s.y), s.r, 3) << s.code;
        EXPECT_NEAR(image.GetGreen(s.x, s.y), s.g, 3) << s.code;
        EXPECT_NEAR(image.GetBlue(s.x, s.y), s.b, 3) << s.code;
    }
    EXPECT_FALSE(flag_icon("xx").IsOk());
}

}  // namespace test_Language
