#include <gtest/gtest.h>

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/image.h>

#include "model/Language.h"
#include "model/Message.h"
#include "ui/MessageLog.h"
#include "ui/Text.h"

#include <string>

#ifdef INDICATOR_GTK3
#include <gtk/gtk.h>
#include <wx/fontutil.h>
#endif

namespace test_MessageLog {

namespace {

struct Tables {
    Tables() {
        add_strings("en", {{"t.hi", "Hi {who}"}, {"t.wrap", "[{inner}]"}});
        add_strings("de", {{"t.hi", "Hallo {who}"}, {"t.wrap", "<{inner}>"}});
        add_strings("ar", {{"t.hi", "مرحبا {who}"}, {"t.wrap", "{inner}"}, {"t.lat", "OK {who}"}});
        add_strings("en", {{"t.lat", "OK {who}"}});
    }
};

/// The language is applied as the window does: the current language, then every line again.
class WxLog : public ::testing::Test {
protected:
    void SetUp() override {
        static Tables tables;
        set_current_language("en");
        frame = new wxFrame(nullptr, wxID_ANY, "t");
        log = new MessageLog(frame);
        log->SetSize(300, 120);
    }
    void TearDown() override {
        frame->Destroy();
        set_current_language("en");
    }
    void apply_language(std::string const& code) {
        set_current_language(code);
        log->rerender();
    }
    wxFrame* frame = nullptr;
    MessageLog* log = nullptr;
};

Message hi(std::string const& who) {
    return Message("t.hi").with("who", who);
}

void mouse(wxWindow* w, wxEventType type, wxPoint p) {
    wxMouseEvent e(type);
    e.SetPosition(p);
    e.SetEventObject(w);
    w->GetEventHandler()->ProcessEvent(e);
}

void drag(wxWindow* w, wxPoint from, wxPoint to) {
    mouse(w, wxEVT_LEFT_DOWN, from);
    mouse(w, wxEVT_MOTION, to);
    mouse(w, wxEVT_LEFT_UP, to);
}

}  // namespace

TEST_F(WxLog, ChangeRerendersOldLines) {
    log->add(hi("/tmp/a.zip"));
    wxString const time = log->line_text(0).Left(8);
    EXPECT_EQ(log->line_text(0), time + "  Hi /tmp/a.zip");
    apply_language("de");
    EXPECT_EQ(log->line_text(0), time + "  Hallo /tmp/a.zip");
    EXPECT_EQ(log->last_message(), "Hallo /tmp/a.zip");
    EXPECT_EQ(log->last_text().key(), "t.hi");
}

TEST_F(WxLog, NestedLinesFollow) {
    log->add(Message("t.wrap").with("inner", hi("x")));
    EXPECT_EQ(log->last_message(), "[Hi x]");
    apply_language("de");
    EXPECT_EQ(log->last_message(), "<Hallo x>");
}

TEST_F(WxLog, ArabicBlocksAreRightToLeft) {
    log->add(hi("a"));
    log->add(hi("b"));
    apply_language("ar");
    EXPECT_EQ(log->GetLayoutDirection(), wxLayout_LeftToRight);
    ASSERT_EQ(log->line_count(), 2u);
    for (std::size_t i = 0; i < log->line_count(); ++i)
        EXPECT_EQ(log->line_direction(i), wxLayout_RightToLeft);
    apply_language("en");
    for (std::size_t i = 0; i < log->line_count(); ++i)
        EXPECT_EQ(log->line_direction(i), wxLayout_LeftToRight);
}

TEST_F(WxLog, ArabicLastMessageHasNoIsolates) {
    log->add(hi("/tmp/a.zip"));
    apply_language("ar");
    EXPECT_NE(log->line_text(0).Find(wxUniChar(0x2066)), wxNOT_FOUND);
    EXPECT_EQ(log->last_message(), wx("مرحبا /tmp/a.zip"));
}

TEST_F(WxLog, FullLogRebuildsInOrder) {
    for (int i = 0; i < MessageLog::max_lines + 5; ++i)
        log->add(hi(std::to_string(i)));
    apply_language("de");
    EXPECT_EQ(log->line_count(), static_cast<std::size_t>(MessageLog::max_lines));
    EXPECT_TRUE(log->line_text(0).EndsWith("Hallo 5"));
    EXPECT_TRUE(log->line_text(log->line_count() - 1).EndsWith("Hallo 1004"));
}

TEST_F(WxLog, DragSelectsAcrossLines) {
    log->add(hi("a"));
    log->add(hi("b"));
    drag(log, log->position_of(0, 12), log->position_of(1, 12));
    EXPECT_EQ(log->selected_text(), log->line_text(0).Mid(12) + "\n" + log->line_text(1).Left(12));
    // a drag within one line, and backwards
    drag(log, log->position_of(1, 13), log->position_of(1, 10));
    EXPECT_EQ(log->selected_text(), "Hi ");
}

TEST_F(WxLog, DoubleClickSelectsAWord) {
    log->add(hi("/tmp/a.zip"));
    mouse(log, wxEVT_LEFT_DCLICK, log->position_of(0, 11) + wxPoint(2, 0));
    EXPECT_EQ(log->selected_text(), "Hi");
}

TEST_F(WxLog, CopyPutsTheSelectionOnTheClipboard) {
    log->add(hi("a"));
    log->add(hi("b"));
    drag(log, log->position_of(0, 10), log->position_of(0, 14));
    ASSERT_EQ(log->selected_text(), "Hi a");
    if (!wxTheClipboard->Open())
        GTEST_SKIP() << "no clipboard";
    wxTheClipboard->Clear();
    wxTheClipboard->Close();
    wxKeyEvent key(wxEVT_KEY_DOWN);
    key.SetControlDown(true);
    key.m_keyCode = 'C';
    key.SetEventObject(log);
    log->GetEventHandler()->ProcessEvent(key);
    wxTextDataObject got;
    ASSERT_TRUE(wxTheClipboard->Open());
    bool const ok = wxTheClipboard->GetData(got);
    wxTheClipboard->Close();
    ASSERT_TRUE(ok);
    EXPECT_EQ(got.GetText(), "Hi a");
}

TEST_F(WxLog, NothingSelectedCopiesNothing) {
    log->add(hi("a"));
    EXPECT_EQ(log->selected_text(), "");
    EXPECT_FALSE(log->copy());
    log->select_all();
    EXPECT_EQ(log->selected_text(), log->line_text(0));
}

TEST_F(WxLog, WheelHandleAndTrackScroll) {
    log->SetSize(300, 60);
    for (int i = 0; i < 20; ++i)
        log->add(hi(std::to_string(i)));
    log->rerender();  // at the bottom at once
    double const range = log->scroll_range();
    ASSERT_GT(range, 0);
    EXPECT_DOUBLE_EQ(log->scroll_position(), range);
    // wheel up
    wxMouseEvent wheel(wxEVT_MOUSEWHEEL);
    wheel.m_wheelRotation = 120;
    wheel.m_wheelDelta = 120;
    wheel.SetEventObject(log);
    log->GetEventHandler()->ProcessEvent(wheel);
    double const after_wheel = log->scroll_position();
    EXPECT_LT(after_wheel, range);
    // dragging the handle up
    wxRect handle = log->handle_rect();
    ASSERT_FALSE(handle.IsEmpty());
    wxPoint const grab(handle.x + 2, handle.y + handle.height / 2);
    drag(log, grab, grab - wxPoint(0, 5));
    EXPECT_LT(log->scroll_position(), after_wheel);
    EXPECT_EQ(log->selected_text(), "");  // it was the handle, not the text
    // a click on the track above the handle pages up
    double const before_page = log->scroll_position();
    handle = log->handle_rect();
    mouse(log, wxEVT_LEFT_DOWN, wxPoint(handle.x + 2, 8));
    mouse(log, wxEVT_LEFT_UP, wxPoint(handle.x + 2, 8));
    EXPECT_LT(log->scroll_position(), before_page);
}

TEST_F(WxLog, DoubleClickStopsAtPunctuation) {
    log->add(hi("/tmp/a.zip"));
    wxString const t = log->line_text(0);
    mouse(log, wxEVT_LEFT_DCLICK, log->position_of(0, t.Find("tmp") + 1));
    EXPECT_EQ(log->selected_text(), "tmp");
}

#ifdef INDICATOR_GTK3  // the portable layout reads right-to-left text in logical order only
TEST_F(WxLog, ArabicLineSelectsByItsLayout) {
    apply_language("ar");
    log->add(hi("a b"));
    wxString const t = log->line_text(0);
    // the line's start is at its right: the caret of offset 0 is right of the one at the end
    EXPECT_GT(log->position_of(0, 0).x, log->position_of(0, t.length()).x);
    // the time stamp is a left-to-right run at the right: a range inside it reads in its own order
    drag(log, log->position_of(0, 2), log->position_of(0, 5));
    EXPECT_EQ(log->selected_text(), t.Mid(2, 3));
    // a range inside the line, both ways round
    drag(log, log->position_of(0, 12), log->position_of(0, 15));
    EXPECT_EQ(log->selected_text(), t.Mid(12, 3));
    drag(log, log->position_of(0, 15), log->position_of(0, 12));
    EXPECT_EQ(log->selected_text(), t.Mid(12, 3));
}
#endif

#ifdef INDICATOR_GTK3  // the portable layout reads right-to-left text in logical order only
TEST_F(WxLog, ArabicLineStartingWithLatinStartsAtTheRight) {
    apply_language("ar");
    log->add(Message("t.lat").with("who", "x"));
    wxString const t = log->line_text(0);
    EXPECT_NE(t.Find("OK"), wxNOT_FOUND);
    EXPECT_GT(log->position_of(0, 0).x, log->position_of(0, t.length()).x);
}
#endif

TEST_F(WxLog, WrappedLineSelectsAcrossItsRows) {
    log->SetSize(130, 120);
    log->add(hi("aaaa bbbb cccc dddd eeee ffff gggg"));
    ASSERT_GT(log->row_count(0), 1);
    wxString const t = log->line_text(0);
    drag(log, log->position_of(0, 3), log->position_of(0, 40));
    EXPECT_EQ(log->selected_text(), t.Mid(3, 37));
    EXPECT_NE(log->position_of(0, 3).y, log->position_of(0, 40).y);
}

TEST_F(WxLog, TabDoesNotStopAtTheLog) {
    EXPECT_FALSE(log->AcceptsFocusFromKeyboard());
}

TEST_F(WxLog, PaintsTextAndSelection) {
    log->add(hi("a"));
    auto count = [&](int r, int g, int b) {
        wxImage const image = log->render(1).ConvertToImage();
        int n = 0;
        for (int y = 0; y < image.GetHeight(); ++y)
            for (int x = 0; x < image.GetWidth(); ++x)
                if (image.GetRed(x, y) == r && image.GetGreen(x, y) == g &&
                    image.GetBlue(x, y) == b)
                    ++n;
        return n;
    };
    EXPECT_GT(count(0x47, 0xFF, 0x86), 0);  // the lit text
    EXPECT_EQ(count(0x1f, 0x5c, 0x35), 0);
    log->select_all();
    EXPECT_GT(count(0x1f, 0x5c, 0x35), 50);  // the selection's face
    EXPECT_GT(count(0xc8, 0xff, 0xd9), 0);   // the selected text
}

TEST_F(WxLog, HighlightLiesUnderTheSelectedGlyphsOfAnArabicLine) {
    apply_language("ar");
    log->add(hi("a b"));
    wxPoint const a = log->position_of(0, 10), b = log->position_of(0, 15);
    int const lo = std::min(a.x, b.x), hi_x = std::max(a.x, b.x);
    drag(log, a, b);
    ASSERT_EQ(log->selected_text(), log->line_text(0).Mid(10, 5));
    wxImage const image = log->render(1).ConvertToImage();
    int inside = 0, outside = 0;
    for (int y = a.y - 5; y <= a.y + 5; ++y) {
        for (int x = 4; x < image.GetWidth() - 12; ++x) {
            if (image.GetRed(x, y) != 0x1f || image.GetGreen(x, y) != 0x5c ||
                image.GetBlue(x, y) != 0x35)
                continue;
            (x >= lo - 1 && x <= hi_x + 1 ? inside : outside) += 1;
        }
    }
    EXPECT_GT(inside, 5);
    EXPECT_EQ(outside, 0);
}

TEST_F(WxLog, ArabicBlocksAreStackedByTheirOwnHeight) {
    apply_language("ar");
    log->SetSize(300, 60);
    for (int i = 0; i < 10; ++i)
        log->add(hi("\u0645\u064e\u0631\u0652\u062d\u064e\u0628\u064b\u0627"));
    log->rerender();
    double total = 0;
    for (std::size_t i = 0; i < log->line_count(); ++i)
        total += log->block_height(i);
    EXPECT_GT(log->block_height(0), 0);
    EXPECT_NEAR(log->position_of(1, 0).y - log->position_of(0, 0).y, log->block_height(0), 1.0);
    EXPECT_NEAR(log->scroll_range(), total - (60 - 2 * 4 - 2 * 2), 0.001);
}

TEST_F(WxLog, DoubleClickKeepsCombiningMarksInTheWord) {
    apply_language("ar");
    std::string const word = "\u0645\u064e\u0631\u0652\u062d\u064e\u0628\u064b\u0627";
    log->add(hi(word));
    wxString const t = log->line_text(0);
    int const at = t.Find(wx(word));
    ASSERT_NE(at, wxNOT_FOUND);
    mouse(log, wxEVT_LEFT_DCLICK, log->position_of(0, static_cast<std::size_t>(at) + 3));
    EXPECT_EQ(log->selected_text(), wx(word));
}

#ifdef INDICATOR_GTK3  // the portable layout reads right-to-left text in logical order only
TEST_F(WxLog, ArabicLineIsRightAligned) {
    apply_language("ar");
    log->add(hi("a b"));
    // the logical start of the line is at the right edge of the text area, the end well to its left
    EXPECT_GT(log->position_of(0, 0).x, log->GetClientSize().x - 40);
    EXPECT_LT(log->position_of(0, log->line_text(0).length()).x, log->position_of(0, 0).x - 60);
    apply_language("en");
    EXPECT_LT(log->position_of(0, 0).x, 30);
}
#endif

#ifdef INDICATOR_GTK3
TEST_F(WxLog, BlockHeightIsTheLayoutsOwnPixelHeight) {
    apply_language("ar");
    std::string const word = "\u0645\u064e\u0631\u0652\u062d\u064e\u0628\u064b\u0627";
    log->add(hi(word));
    PangoLayout* layout =
        gtk_widget_create_pango_layout(static_cast<GtkWidget*>(log->GetHandle()), nullptr);
    pango_layout_set_font_description(layout, log->text_font().GetNativeFontInfo()->description);
    std::string const text = log->line_text(0).utf8_string();
    pango_layout_set_text(layout, text.data(), static_cast<int>(text.size()));
    int width = 0, height = 0;
    pango_layout_get_pixel_size(layout, &width, &height);
    g_object_unref(layout);
    EXPECT_NEAR(log->block_height(0), height, 1.0);
}
#endif

}  // namespace test_MessageLog
