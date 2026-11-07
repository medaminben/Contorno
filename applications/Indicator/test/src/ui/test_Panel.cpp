#include <gtest/gtest.h>

#include <wx/frame.h>
#include <wx/image.h>
#include <wx/statbmp.h>

#include "model/Language.h"
#include "model/Message.h"
#include "ui/ControlPanel.h"
#include "ui/Localise.h"
#include "ui/PhotoView.h"
#include "ui/Text.h"
#include "ui/Theme.h"
#include "ui/WxTestApp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace test_Panel {

namespace {

using namespace std::chrono_literals;

/// Back to English and the dark theme when a test ends, whatever it did.
struct Restore {
    ~Restore() {
        apply_language("en");
        set_light_theme(false);
    }
};

/// Counts the paints of a window while it lives (the window may outlive it: a frame is destroyed when idle).
struct PaintCounter {
    int count = 0;
    wxWindow* window;
    explicit PaintCounter(wxWindow* w) : window(w) { window->Bind(wxEVT_PAINT, &PaintCounter::on_paint, this); }
    ~PaintCounter() { window->Unbind(wxEVT_PAINT, &PaintCounter::on_paint, this); }
    PaintCounter(PaintCounter const&) = delete;
    PaintCounter& operator=(PaintCounter const&) = delete;
    void on_paint(wxPaintEvent& e) {
        ++count;
        e.Skip();
    }
};

/// A panel in a frame, sized to the frame's width and its own height.
struct Fixture {
    Restore restore;
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    ControlPanel* panel = new ControlPanel(frame);
    Fixture() {
        frame->SetClientSize(1600, panel->GetBestSize().y);
        panel->SetSize(frame->GetClientSize());
        panel->Layout();
    }
    ~Fixture() { frame->Destroy(); }
    std::vector<Key*> keys() const {
        std::vector<Key*> out;
        collect_descendants<Key>(panel, out);
        return out;
    }
    /// Where the panel's parts are, in the panel's coordinates.
    std::vector<wxRect> geometry() const {
        std::vector<wxRect> out;
        for (Key* key : keys()) out.push_back(key->GetRect());
        for (wxWindow* part : {static_cast<wxWindow*>(&panel->display()), static_cast<wxWindow*>(&panel->log()),
                               static_cast<wxWindow*>(&panel->mask())})
            out.push_back(part->GetRect());
        return out;
    }
};

wxImage solid(int w, int h, unsigned char r, unsigned char g, unsigned char b) {
    wxImage image(w, h);
    image.SetRGB(wxRect(0, 0, w, h), r, g, b);
    return image;
}

struct ViewFixture {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    PhotoView* view = new PhotoView(frame);
    ViewFixture() { view->SetSize(200, 100); }
    ~ViewFixture() { frame->Destroy(); }
};

wxColour pixel(wxBitmap const& bitmap, int x, int y) {
    wxImage const image = bitmap.ConvertToImage();
    return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
}

}  // namespace

// ------------------------------------------------------------------ ControlPanel

TEST(WxPanel, KeyLabelsFitInEveryLanguage) {
    Fixture f;
    ASSERT_EQ(f.keys().size(), 4u);
    for (auto const& code : language_codes()) {
        apply_language(code);
        auto const m = control_panel_metrics_for(f.panel);
        int const inset = std::max(2, static_cast<int>(std::lround(4 * m.scale)));
        for (Key* key : f.keys()) {
            int w = 0, h = 0;
            key->GetTextExtent(key->GetLabelText(), &w, &h);
            EXPECT_LE(w, key->GetSize().x - inset) << code << " " << utf8(key->GetLabelText());
            EXPECT_FALSE(key->GetLabelText().empty()) << code;
        }
    }
}

TEST(WxPanel, ControlPanelScaleFollowsTheReferenceScreen) {
    auto const full = control_panel_metrics(1920, 1080);
    EXPECT_DOUBLE_EQ(full.scale, 1);
    EXPECT_EQ(full.height, 140);
    EXPECT_EQ(full.wordmark, 110);
    EXPECT_EQ(full.gap, 6);
    EXPECT_EQ(full.margin, 9);
    EXPECT_EQ(full.spacer, 22);
    EXPECT_EQ(full.log_min_width, 320);
    EXPECT_EQ(full.label_px, 16);
    EXPECT_EQ(full.key_side, 67);  // (140 − 6) / 2

    // 1368/1920 = 0.7125, 768/1080 = 0.7111…, so the height binds
    auto const laptop = control_panel_metrics(1368, 768);
    EXPECT_DOUBLE_EQ(laptop.scale, 768.0 / 1080.0);
    EXPECT_EQ(laptop.height, static_cast<int>(std::lround(140 * laptop.scale)));
    EXPECT_EQ(laptop.wordmark, static_cast<int>(std::lround(110 * laptop.scale)));
    EXPECT_EQ(laptop.gap, static_cast<int>(std::lround(6 * laptop.scale)));
    EXPECT_EQ(laptop.margin, static_cast<int>(std::lround(9 * laptop.scale)));
    EXPECT_EQ(laptop.spacer, static_cast<int>(std::lround(22 * laptop.scale)));
    EXPECT_EQ(laptop.log_min_width, static_cast<int>(std::lround(320 * laptop.scale)));
    EXPECT_EQ(laptop.label_px, static_cast<int>(std::lround(16 * laptop.scale)));
    EXPECT_EQ(laptop.key_side, (laptop.height - laptop.gap) / 2);

    // a wide short screen is limited by the height; a narrow tall one by the width
    EXPECT_DOUBLE_EQ(control_panel_metrics(1920, 768).scale, 768.0 / 1080.0);
    EXPECT_DOUBLE_EQ(control_panel_metrics(1368, 1080).scale, 1368.0 / 1920.0);
    EXPECT_DOUBLE_EQ(control_panel_metrics(3840, 2160).scale, 2);
}

TEST(WxPanel, SizesFollowTheScreenTheWindowIsOn) {
    Fixture f;
    auto const m = control_panel_metrics_for(f.panel);
    EXPECT_EQ(f.keys()[0]->GetMinSize(), wxSize(m.key_side, m.key_side));
    EXPECT_EQ(f.panel->mask().GetMinSize(), wxSize(m.height, m.height));
    EXPECT_EQ(f.panel->display().GetMinSize().y, m.height);
    EXPECT_EQ(f.panel->log().GetMinSize().GetWidth(), m.log_min_width);
    wxStaticBitmap* logo = find_descendant<wxStaticBitmap>(f.panel);
    ASSERT_NE(logo, nullptr);
    EXPECT_EQ(logo->GetBitmap().GetLogicalHeight(), m.wordmark);
}

// The Qt key draws "Import" with a 16 px em (QFont::setPixelSize): its ink 16 px high, 51-53 px wide on the
// 67 px key. wxFont::SetPixelSize would make the whole line 16 px high, the ink only 11.
TEST(WxPanel, KeyLabelIsAsLargeAsQts) {
    Fixture f;
    Key* import = f.keys()[0];
    auto const m = control_panel_metrics_for(f.panel);
    ASSERT_EQ(import->GetSize(), wxSize(m.key_side, m.key_side));
    // The Qt key's ink is measured on the 1920×1080 key (67 px, 16 px em).
    if (m.key_side != 67) return;
    ASSERT_EQ(utf8(import->GetLabelText()), "Import");
    wxImage const image = import->Render(1).ConvertToImage();
    int top = image.GetHeight(), bottom = -1, left = image.GetWidth(), right = -1;
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            if (image.GetGreen(x, y) - image.GetRed(x, y) <= 60) continue;  // the text's green, not the grey face
            top = std::min(top, y);
            bottom = std::max(bottom, y);
            left = std::min(left, x);
            right = std::max(right, x);
        }
    }
    ASSERT_GE(bottom, top);
    EXPECT_NEAR(bottom - top + 1, 16, 1);  // the Qt key's, measured with the same rule
    EXPECT_NEAR(right - left + 1, 52, 2);
}

TEST(WxPanel, KeysAreInOrderWithTheirIdsAndTexts) {
    Fixture f;
    auto const keys = f.keys();
    ASSERT_EQ(keys.size(), 4u);
    char const* names[] = {"panel.import", "panel.open", "panel.save", "panel.setup"};
    int const ids[] = {ID_IMPORT, ID_OPEN, ID_SAVE, ID_SETUP};
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(keys[i]->GetId(), ids[i]);
        EXPECT_EQ(utf8(keys[i]->GetLabelText()), lookup("en", names[i]));
        EXPECT_EQ(utf8(keys[i]->GetToolTipText()), lookup("en", std::string(names[i]) + ".tip"));
        auto const m = control_panel_metrics_for(f.panel);
        EXPECT_EQ(keys[i]->GetSize(), wxSize(m.key_side, m.key_side));
    }
}

TEST(WxPanel, AClickReachesThePanelWithTheKeysId) {
    Fixture f;
    std::vector<int> seen;
    f.panel->Bind(wxEVT_BUTTON, [&](wxCommandEvent& e) { seen.push_back(e.GetId()); });
    for (Key* key : f.keys()) key->Click();
    EXPECT_EQ(seen, (std::vector<int> {ID_IMPORT, ID_OPEN, ID_SAVE, ID_SETUP}));
}

TEST(WxPanel, LabelsAndTipsFollowTheLanguage) {
    Fixture f;
    apply_language("de");
    auto const keys = f.keys();
    EXPECT_EQ(utf8(keys[1]->GetLabelText()), lookup("de", "panel.open"));
    EXPECT_NE(lookup("de", "panel.open"), lookup("en", "panel.open"));
    EXPECT_EQ(utf8(keys[1]->GetToolTipText()), lookup("de", "panel.open.tip"));
    apply_language("ar");
    EXPECT_EQ(utf8(keys[3]->GetLabelText()), lookup("ar", "panel.setup"));
    EXPECT_EQ(keys[3]->GetLayoutDirection(), wxLayout_RightToLeft);  // the key's own text turns
}

TEST(WxPanel, StaysLeftToRightInArabic) {
    Fixture f;
    auto const before = f.geometry();
    apply_language("ar");
    f.panel->Layout();
    EXPECT_EQ(f.panel->GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(f.geometry(), before);
    // the order left to right is the wordmark, the keys, the mask, the log, the readout
    EXPECT_LT(before[0].x, before[2].x);  // Import left of Save
    EXPECT_LT(before[6].x, before[5].x);  // mask left of the log
    EXPECT_LT(before[5].x, before[4].x);  // log left of the readout
}

// Switched to Arabic while open, the readout, the log and the mask stay left to right (a mouse position in a
// right-to-left window counts from its right edge), and GTK's mirrored context of a right-to-left window never
// shows: the screen shows what they draw
TEST(WxPanel, DrawnPartsAreNotMirroredAfterASwitchToArabic) {
    Fixture f;
    f.frame->Show();
    apply_language("ar");
    EXPECT_EQ(f.panel->display().GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(f.panel->log().GetLayoutDirection(), wxLayout_LeftToRight);
    EXPECT_EQ(f.panel->mask().GetLayoutDirection(), wxLayout_LeftToRight);
    f.panel->log().add(Message("panel.import"));
    pump_until([&] { return !f.panel->log().gliding(); }, 2000ms);
    for (wxWindow* part : {static_cast<wxWindow*>(&f.panel->display()), static_cast<wxWindow*>(&f.panel->log())}) {
        wxRect const all(wxPoint(0, 0), part->GetSize());
        Ink const shown = ink_on_screen(*part, all);
        if (!shown.read) GTEST_SKIP() << "the screen cannot be read here";
        ASSERT_GT(shown.count, 0);
        wxImage const drawn = dynamic_cast<SegmentDisplay*>(part) ? f.panel->display().render(1).ConvertToImage()
                                                                   : f.panel->log().render(1).ConvertToImage();
        EXPECT_LT(ink_difference(shown, ink_of(drawn, all)), 0.05);
    }
}

// whatever turns a drawn part right to left, it paints as Qt's do: never mirrored
TEST(WxPanel, DrawnPartsPaintUnmirroredWhenRightToLeft) {
    Fixture f;
    f.frame->Show();
    f.panel->log().add(Message("panel.import"));
    pump_until([&] { return !f.panel->log().gliding(); }, 2000ms);
    PaintCounter display_paints(&f.panel->display()), log_paints(&f.panel->log());
    for (wxWindow* part : {static_cast<wxWindow*>(&f.panel->display()), static_cast<wxWindow*>(&f.panel->log())}) {
        PaintCounter& paints = part == &f.panel->display() ? display_paints : log_paints;
        part->SetLayoutDirection(wxLayout_RightToLeft);
        // read only what was painted right to left, not the frame from before
        paints.count = 0;
        part->Refresh();
        ASSERT_TRUE(pump_until([&] { return paints.count > 0; }, 3000ms));
        wxRect const all(wxPoint(0, 0), part->GetSize());
        Ink const shown = ink_on_screen(*part, all);
        if (!shown.read) GTEST_SKIP() << "the screen cannot be read here";
        ASSERT_GT(shown.count, 0);
        wxImage const drawn = dynamic_cast<SegmentDisplay*>(part) ? f.panel->display().render(1).ConvertToImage()
                                                                   : f.panel->log().render(1).ConvertToImage();
        EXPECT_LT(ink_difference(shown, ink_of(drawn, all)), 0.05);
    }
}

TEST(WxPanel, HasTheWordmarkAtItsHeight) {
    Fixture f;
    wxStaticBitmap* logo = find_descendant<wxStaticBitmap>(f.panel);
    ASSERT_NE(logo, nullptr);
    ASSERT_TRUE(logo->GetBitmap().IsOk());
    auto const m = control_panel_metrics_for(f.panel);
    EXPECT_EQ(logo->GetBitmap().GetLogicalHeight(), m.wordmark);
    EXPECT_NEAR(logo->GetBitmap().GetLogicalWidth(), m.wordmark * 2125.0 / 725, 1.0);
}

TEST(WxPanel, WordmarkIsWhiteOnDarkAndBlackOnLight) {
    Fixture f;
    auto opaque_colour = [&] {
        wxImage const image = find_descendant<wxStaticBitmap>(f.panel)->GetBitmap().ConvertToImage();
        for (int y = 0; y < image.GetHeight(); ++y)
            for (int x = 0; x < image.GetWidth(); ++x)
                if (image.HasAlpha() && image.GetAlpha(x, y) == 255) return wxColour(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
        return wxColour(1, 2, 3);
    };
    apply_theme(f.frame, false);
    EXPECT_EQ(opaque_colour(), wxColour(255, 255, 255));
    apply_theme(f.frame, true);
    EXPECT_EQ(opaque_colour(), wxColour(0, 0, 0));
    apply_theme(f.frame, false);
    EXPECT_EQ(opaque_colour(), wxColour(255, 255, 255));
}

TEST(WxPanel, ReadoutCornerFollowsTheTheme) {
    Fixture f;
    apply_theme(f.frame, false);
    EXPECT_EQ(pixel(f.panel->display().render(1), 0, 0), palette(false).panel);
    apply_theme(f.frame, true);
    EXPECT_EQ(pixel(f.panel->display().render(1), 0, 0), palette(true).panel);
    EXPECT_NE(palette(true).panel, palette(false).panel);
}

namespace {
/// Counts the paints a window receives (the original handler still runs).
}  // namespace

// the window recolours its panel, not the keys (MainWindow::apply_theme): built after a light window, the keys'
// corners must still show the black panel, not white specks
TEST(WxPanel, KeyCornersShowThePanelsColour) {
    set_light_theme(true);
    Fixture f;
    f.panel->SetBackgroundColour(palette(false).panel);
    for (Key* key : f.keys()) {
        wxImage const image = key->Render(1).ConvertToImage();
        for (wxPoint corner : {wxPoint(0, 0), wxPoint(image.GetWidth() - 1, 0), wxPoint(0, image.GetHeight() - 1),
                               wxPoint(image.GetWidth() - 1, image.GetHeight() - 1)}) {
            wxColour const shown(image.GetRed(corner.x, corner.y), image.GetGreen(corner.x, corner.y),
                                 image.GetBlue(corner.x, corner.y));
            EXPECT_EQ(shown.GetAsString(wxC2S_HTML_SYNTAX), "#000000") << corner.x << "," << corner.y;
        }
    }
}

// a drawn child shows its parent's colour in its corners, so it must be painted again when that colour changes
TEST(WxPanel, DrawnChildrenRepaintWhenTheParentColourChanges) {
    Fixture f;
    f.frame->Show();
    PaintCounter display(&f.panel->display()), log(&f.panel->log());
    pump_until([] { return false; }, 300ms);
    f.panel->Refresh();
    f.panel->Update();
    ASSERT_GT(display.count, 0);
    ASSERT_GT(log.count, 0);
    display.count = log.count = 0;
    f.panel->Update();
    ASSERT_EQ(display.count + log.count, 0);  // nothing is invalid until the colour changes
    f.panel->SetBackgroundColour(wxColour(200, 50, 50));  // not through apply_theme
    f.panel->Update();  // paints what is invalid now
    EXPECT_GT(display.count, 0);
    EXPECT_GT(log.count, 0);
    EXPECT_EQ(pixel(f.panel->display().render(1), 0, 0), wxColour(200, 50, 50));
    EXPECT_EQ(pixel(f.panel->log().render(1), 0, 0), wxColour(200, 50, 50));
}

// ------------------------------------------------------------------ PhotoView

namespace {

Vision::PixelFrame frame_of(wxImage const& image) { return {image.GetWidth(), image.GetHeight(), 1.0, 1.0}; }

void set_photo(PhotoView& view, wxImage const& image, bool keep_view = false) {
    view.set_photo(image, frame_of(image), keep_view);
}

/// A rectangle (x0, y0)-(x1, y1) in mm, as one polygon.
Pot::Polygon64 rectangle(double x0, double y0, double x1, double y1) {
    auto p = [](double x, double y) { return Pot::Point64 {std::llround(x * Pot::units_per_mm), std::llround(y * Pot::units_per_mm)}; };
    return {{p(x0, y0), p(x1, y0), p(x1, y1), p(x0, y1)}, {}};
}

void wheel(PhotoView& view, int rotation, wxPoint at = {1, 1}) {
    wxMouseEvent event(wxEVT_MOUSEWHEEL);
    event.SetPosition(at);
    event.m_wheelRotation = rotation;
    event.m_wheelDelta = 120;
    view.GetEventHandler()->ProcessEvent(event);
}

void drag(PhotoView& view, wxPoint from, wxPoint to) {
    wxMouseEvent down(wxEVT_LEFT_DOWN);
    down.SetPosition(from);
    view.GetEventHandler()->ProcessEvent(down);
    wxMouseEvent move(wxEVT_MOTION);
    move.SetPosition(to);
    move.SetLeftDown(true);
    view.GetEventHandler()->ProcessEvent(move);
    wxMouseEvent up(wxEVT_LEFT_UP);
    up.SetPosition(to);
    view.GetEventHandler()->ProcessEvent(up);
}

struct TwoViews {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    PhotoView* a = new PhotoView(frame);
    PhotoView* b = new PhotoView(frame);
    TwoViews() {
        for (PhotoView* v : {a, b}) {
            v->SetSize(100, 100);
            set_photo(*v, solid(100, 100, 255, 0, 0));
        }
    }
    ~TwoViews() { frame->Destroy(); }
};

}  // namespace

TEST(WxPhotoView, FitsThePhotoAndKeepsFittingOnResize) {
    ViewFixture f;
    set_photo(*f.view, solid(100, 50, 255, 0, 0));
    EXPECT_TRUE(f.view->fitted());
    EXPECT_DOUBLE_EQ(f.view->zoom(), 2.0);
    f.view->SetSize(400, 400);
    EXPECT_DOUBLE_EQ(f.view->zoom(), 4.0);
    wxImage const drawn = f.view->render(1).ConvertToImage();
    EXPECT_EQ(drawn.GetRed(200, 200), 255);                      // the photo
    EXPECT_EQ(drawn.GetRed(200, 20), 60);                        // above it: the backdrop
}

TEST(WxPhotoView, WheelZoomsAboutTheViewCentre) {
    ViewFixture f;
    f.view->SetSize(100, 100);
    set_photo(*f.view, solid(100, 100, 255, 0, 0));
    // the cursor is elsewhere: the Qt view's anchor is the view's centre (of the view as it is before the step)
    for (int i = 0; i < 5; ++i) {
        wxSize const size = f.view->GetClientSize();
        wxPoint const centre(size.x / 2, size.y / 2);
        wxPoint2DDouble const under = f.view->image_point_at(centre);
        double const before = f.view->zoom();
        wheel(*f.view, 120, wxPoint(10, 90));
        EXPECT_NEAR(f.view->zoom(), before * 1.15, 1e-9);
        wxPoint2DDouble const now = f.view->image_point_at(centre);
        EXPECT_NEAR(now.m_x, under.m_x, 1.5) << i;
        EXPECT_NEAR(now.m_y, under.m_y, 1.5) << i;
    }
    EXPECT_FALSE(f.view->fitted());
    EXPECT_NEAR(f.view->zoom(), std::pow(1.15, 5), 1e-9);
    for (int i = 0; i < 5; ++i) wheel(*f.view, -120, wxPoint(10, 90));
    EXPECT_NEAR(f.view->zoom(), 1.0, 1e-9);
    EXPECT_FALSE(f.view->fitted());  // zoomed once: it stays so
}

TEST(WxPhotoView, ResizeKeepsTheZoomOnceZoomed) {
    ViewFixture f;
    set_photo(*f.view, solid(100, 100, 255, 0, 0));
    wheel(*f.view, 120);
    double const zoom = f.view->zoom();
    f.view->SetSize(300, 300);
    EXPECT_FALSE(f.view->fitted());
    EXPECT_DOUBLE_EQ(f.view->zoom(), zoom);
}

TEST(WxPhotoView, DragPans) {
    ViewFixture f;
    f.frame->Show();
    ASSERT_TRUE(pump_until([&] { return f.view->IsShownOnScreen(); }, 3000ms));  // the drag grabs the pointer
    f.view->SetSize(100, 100);
    set_photo(*f.view, solid(100, 100, 255, 0, 0));
    f.view->zoom_at(wxPoint(50, 50), 4.0);
    wxPoint const start = f.view->view_origin();
    wxMouseEvent down(wxEVT_LEFT_DOWN);
    down.SetPosition(wxPoint(60, 60));
    f.view->GetEventHandler()->ProcessEvent(down);
    wxMouseEvent move(wxEVT_MOTION);
    move.SetPosition(wxPoint(40, 50));
    move.SetLeftDown(true);
    f.view->GetEventHandler()->ProcessEvent(move);
    wxPoint const moved = f.view->view_origin();
    EXPECT_EQ(moved.x, start.x + 20);  // the photo follows the hand
    EXPECT_EQ(moved.y, start.y + 10);
    wxMouseEvent up(wxEVT_LEFT_UP);
    up.SetPosition(wxPoint(40, 50));
    f.view->GetEventHandler()->ProcessEvent(up);
    EXPECT_FALSE(f.view->HasCapture());
    move.SetLeftDown(false);
    move.SetPosition(wxPoint(10, 10));
    f.view->GetEventHandler()->ProcessEvent(move);
    EXPECT_EQ(f.view->view_origin(), moved);
}

TEST(WxPhotoView, KeepViewKeepsTheZoomForTheSameSize) {
    ViewFixture f;
    f.view->SetSize(100, 100);  // zoomed, the photo is larger than the view both ways
    set_photo(*f.view, solid(100, 100, 255, 0, 0));
    wheel(*f.view, 120);
    wheel(*f.view, 120);
    f.view->Scroll(7, 5);
    wxPoint const origin = f.view->view_origin();
    ASSERT_EQ(origin, wxPoint(7, 5));
    double const zoom = f.view->zoom();
    set_photo(*f.view, solid(100, 100, 0, 0, 255), true);
    EXPECT_DOUBLE_EQ(f.view->zoom(), zoom);
    EXPECT_EQ(f.view->view_origin(), origin);
    EXPECT_FALSE(f.view->fitted());
    set_photo(*f.view, solid(50, 50, 0, 0, 255), true);  // another size: fitted again
    EXPECT_TRUE(f.view->fitted());
    EXPECT_DOUBLE_EQ(f.view->zoom(), 2.0);
    wheel(*f.view, 120);
    set_photo(*f.view, solid(50, 50, 0, 0, 255));  // not asked to keep: fitted again
    EXPECT_TRUE(f.view->fitted());
}

TEST(WxPhotoView, LinkedViewsZoomAndScrollTogether) {
    TwoViews f;
    f.frame->Show();
    ASSERT_TRUE(pump_until([&] { return f.a->IsShownOnScreen() && f.b->IsShownOnScreen(); }, 3000ms));
    f.a->link_to(f.b);
    double notified = 0;
    f.b->on_zoom_changed([&](double zoom, bool zoomed) { notified = zoomed ? zoom : -1; });
    wheel(*f.a, 120);
    wheel(*f.a, 120);
    EXPECT_FALSE(f.b->fitted());
    EXPECT_DOUBLE_EQ(f.b->zoom(), f.a->zoom());
    EXPECT_DOUBLE_EQ(notified, f.a->zoom());
    EXPECT_EQ(f.b->view_origin(), f.a->view_origin());
    EXPECT_NE(f.a->view_origin(), wxPoint(0, 0));  // a pair that never scrolled would be equal anyway
}

TEST(WxPhotoView, LinkedViewsFollowTheirPan) {
    TwoViews f;
    f.frame->Show();
    ASSERT_TRUE(pump_until([&] { return f.a->IsShownOnScreen() && f.b->IsShownOnScreen(); }, 3000ms));
    f.a->link_to(f.b);
    f.a->zoom_at(wxPoint(50, 50), 4.0);
    drag(*f.a, wxPoint(60, 60), wxPoint(40, 50));
    EXPECT_EQ(f.b->view_origin(), f.a->view_origin());
    wxPoint const after_a = f.a->view_origin();
    drag(*f.b, wxPoint(40, 40), wxPoint(50, 45));
    EXPECT_EQ(f.a->view_origin(), f.b->view_origin());
    EXPECT_NE(f.a->view_origin(), after_a);
}

// the scroll bars' events as GTK delivers them: through the window's handler chain, scroll helper first
TEST(WxPhotoView, LinkedViewsFollowScrollBarDrags) {
    TwoViews f;
    f.frame->Show();
    ASSERT_TRUE(pump_until([&] { return f.a->IsShownOnScreen() && f.b->IsShownOnScreen(); }, 3000ms));
    f.a->link_to(f.b);
    f.a->zoom_at(wxPoint(50, 50), 4.0);
    ASSERT_EQ(f.a->view_origin(), f.b->view_origin());
    struct Step {
        wxEventType type;
        int position;
    };
    for (Step const step : {Step {wxEVT_SCROLLWIN_THUMBTRACK, 30}, Step {wxEVT_SCROLLWIN_THUMBRELEASE, 31},
                            Step {wxEVT_SCROLLWIN_LINEDOWN, 0}, Step {wxEVT_SCROLLWIN_PAGEDOWN, 0},
                            Step {wxEVT_SCROLLWIN_THUMBTRACK, 3}}) {
        for (PhotoView* view : {f.a, f.b}) {
            PhotoView* other = view == f.a ? f.b : f.a;
            wxPoint const before = view->view_origin();
            wxScrollWinEvent event(step.type, step.position, wxHORIZONTAL);
            event.SetEventObject(view);
            view->ProcessWindowEvent(event);
            // the follow may come once the event is through
            pump_until([&] { return other->view_origin() == view->view_origin(); }, 500ms);
            EXPECT_EQ(other->view_origin(), view->view_origin()) << step.type << " " << step.position;
            (void)before;
        }
    }
    EXPECT_NE(f.a->view_origin(), wxPoint(0, 0));
}

TEST(WxPhotoView, LinkedViewFollowsTheClampOfAResize) {
    TwoViews f;
    f.frame->Show();
    ASSERT_TRUE(pump_until([&] { return f.a->IsShownOnScreen() && f.b->IsShownOnScreen(); }, 3000ms));
    f.a->link_to(f.b);
    f.a->zoom_at(wxPoint(50, 50), 4.0);
    f.a->Scroll(1000, 1000);  // as far as it goes
    f.a->zoom_at(f.a->GetClientSize().x > 0 ? wxPoint(50, 50) : wxPoint(), 1.0);  // tells the other
    wxPoint const far = f.a->view_origin();
    ASSERT_EQ(f.b->view_origin(), far);
    f.a->SetSize(300, 300);  // a larger view: the scroll range shrinks and the origin is clamped
    pump_until([&] { return f.a->view_origin() != far; }, 1000ms);
    ASSERT_NE(f.a->view_origin(), far);
    EXPECT_EQ(f.b->view_origin(), f.a->view_origin());
}

TEST(WxPhotoView, PolygonLayersShowInRenderImage) {
    ViewFixture f;
    set_photo(*f.view, solid(100, 100, 0, 0, 255), false);
    EXPECT_EQ(f.view->polygon_layers(), 0u);
    wxImage const plain = f.view->render_image();
    ASSERT_TRUE(plain.IsOk());
    EXPECT_EQ(plain.GetSize(), wxSize(100, 100));
    // mm (20, 10)-(80, 30) with y up: the top edge is at row 70 (100 - 30), the bottom one at row 90
    f.view->add_polygons({rectangle(20, 10, 80, 30)}, wxPen(wxColour(255, 0, 255), 2), *wxTRANSPARENT_BRUSH);
    EXPECT_EQ(f.view->polygon_layers(), 1u);
    wxImage const with = f.view->render_image();
    EXPECT_EQ(with.GetRed(50, 70), 255);   // the pen's colour on the photo's blue
    EXPECT_EQ(with.GetBlue(50, 70), 255);
    EXPECT_EQ(with.GetGreen(50, 70), 0);
    EXPECT_EQ(with.GetRed(50, 90), 255);
    EXPECT_EQ(with.GetRed(50, 10), 0);     // not mirrored: nothing near the bottom edge of the image's top
    EXPECT_EQ(with.GetRed(50, 30), 0);
    EXPECT_EQ(with.GetRed(50, 80), 0);     // inside: the photo (no fill)
    f.view->set_layer_visible(0, false);
    EXPECT_EQ(f.view->render_image().GetRed(50, 70), 0);
    f.view->set_layer_visible(0, true);
    EXPECT_EQ(f.view->render_image().GetRed(50, 70), 255);
    f.view->remove_polygons(0);
    EXPECT_EQ(f.view->polygon_layers(), 0u);
    EXPECT_EQ(f.view->render_image().GetRed(50, 70), 0);
}

TEST(WxPhotoView, LayersStackAndRemoveFromAGivenOne) {
    ViewFixture f;
    set_photo(*f.view, solid(100, 100, 0, 0, 255));
    f.view->add_polygons({rectangle(20, 10, 80, 30)}, wxPen(wxColour(255, 0, 0), 2), *wxTRANSPARENT_BRUSH);
    f.view->add_polygons({rectangle(20, 50, 80, 70)}, wxPen(wxColour(0, 255, 0), 2), *wxTRANSPARENT_BRUSH);
    EXPECT_EQ(f.view->polygon_layers(), 2u);
    f.view->remove_polygons(1);
    EXPECT_EQ(f.view->polygon_layers(), 1u);
    wxImage const image = f.view->render_image();
    EXPECT_EQ(image.GetRed(50, 70), 255);
    EXPECT_EQ(image.GetGreen(50, 50), 0);  // the second layer's top edge (row 50) is gone
    f.view->clear();
    EXPECT_EQ(f.view->polygon_layers(), 0u);
    EXPECT_FALSE(f.view->has_photo());
    EXPECT_FALSE(f.view->render_image().IsOk());  // without a photo: nothing
}

TEST(WxPhotoView, FilledPolygonWithAHoleIsOddEven) {
    ViewFixture f;
    set_photo(*f.view, solid(100, 100, 0, 0, 255));
    Pot::Polygon64 ring = rectangle(10, 10, 90, 90);
    ring.holes.push_back(rectangle(40, 40, 60, 60).outer);
    f.view->add_polygons({ring}, wxPen(wxColour(0, 0, 0), 1, wxPENSTYLE_TRANSPARENT), wxBrush(wxColour(255, 0, 0)));
    wxImage const image = f.view->render_image();
    EXPECT_EQ(image.GetRed(20, 50), 255);  // filled
    EXPECT_EQ(image.GetRed(50, 50), 0);    // the hole shows the photo
}

// ------------------------------------------------------------------ mask_image

TEST(WxMaskImage, IsGrey) {
    Vision::Mask mask {3, 2, {0, 255, 0, 255, 0, 0}};
    wxImage const image = mask_image(mask);
    ASSERT_TRUE(image.IsOk());
    ASSERT_EQ(image.GetSize(), wxSize(3, 2));
    EXPECT_EQ(image.GetRed(0, 0), 0);
    EXPECT_EQ(image.GetRed(1, 0), 255);
    EXPECT_EQ(image.GetGreen(0, 1), 255);
    EXPECT_EQ(image.GetBlue(2, 1), 0);
    EXPECT_EQ(image.GetBlue(1, 0), 255);
}

TEST(WxMaskImage, EmptyOrInconsistentIsInvalid) {
    EXPECT_FALSE(mask_image(Vision::Mask {}).IsOk());
    EXPECT_FALSE(mask_image(Vision::Mask {0, 3, {}}).IsOk());
    EXPECT_FALSE(mask_image(Vision::Mask {2, 2, {0, 255, 0}}).IsOk());  // too few values
    EXPECT_FALSE(mask_image(Vision::Mask {2, 1, {0, 255, 0}}).IsOk());  // too many
    EXPECT_FALSE(mask_image(Vision::Mask {-2, -1, {0, 255}}).IsOk());
}

}  // namespace test_Panel
