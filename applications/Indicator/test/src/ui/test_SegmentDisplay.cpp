#include <gtest/gtest.h>

#include <wx/frame.h>
#include <wx/image.h>

#include "model/Language.h"
#include "ui/SegmentDisplay.h"
#include "ui/WxTestApp.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace test_SegmentDisplay {

namespace {

/// A display in a frame, sized to its best size.
struct Fixture {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    SegmentDisplay* display = new SegmentDisplay(frame);
    Fixture() { display->SetSize(display->GetBestSize()); }
    ~Fixture() { frame->Destroy(); }
};

/// True when a pixel of `image` within columns [x0, x1) and rows [y0, y1)
/// lies within `max_distance` of (r, g, b) in every channel.
bool has_colour_near(wxImage const& image, int r, int g, int b, int max_distance, int x0 = 0, int x1 = -1, int y0 = 0,
                     int y1 = -1) {
    if (x1 < 0) x1 = image.GetWidth();
    if (y1 < 0) y1 = image.GetHeight();
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (std::abs(image.GetRed(x, y) - r) <= max_distance && std::abs(image.GetGreen(x, y) - g) <= max_distance &&
                std::abs(image.GetBlue(x, y) - b) <= max_distance)
                return true;
        }
    }
    return false;
}

/// The values shown while counting, until the count ends (at most 3 s).
std::vector<double> count_steps(SegmentDisplay const& d) {
    std::vector<double> steps;
    auto const end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (d.counting() && std::chrono::steady_clock::now() < end) {
        steps.push_back(std::stod(d.shown_text().ToStdString()));
        auto const before = std::chrono::steady_clock::now();
        pump_until([&] { return !d.counting() || std::chrono::steady_clock::now() - before >= std::chrono::milliseconds(20); },
                   std::chrono::milliseconds(100));
    }
    return steps;
}

}  // namespace

TEST(WxSegmentDisplay, ShowsValueAndUnit) {
    Fixture f;
    f.display->set_value("1.842", wxString::FromUTF8("m²"));
    EXPECT_EQ(f.display->text(), "1.842");
    EXPECT_EQ(f.display->unit(), wxString::FromUTF8("m²"));
    EXPECT_FALSE(f.display->dimmed());
}

// a new value is counted to from the one shown: up when larger, down when smaller
TEST(WxSegmentDisplay, CountsInTheChangesDirection) {
    Fixture f;
    SegmentDisplay& d = *f.display;
    wxString const m2 = wxString::FromUTF8("m²"), cm2 = wxString::FromUTF8("cm²");
    d.set_value("1.000", m2);
    EXPECT_EQ(d.text(), "1.000");  // the value at once, the cells follow
    EXPECT_TRUE(d.counting());
    auto up = count_steps(d);
    ASSERT_GE(up.size(), 3u);
    EXPECT_LT(up.front(), 1.0);
    EXPECT_TRUE(std::is_sorted(up.begin(), up.end()));
    EXPECT_FALSE(d.counting());
    EXPECT_EQ(d.shown_text(), "1.000");

    d.set_value("0.400", m2);
    auto down = count_steps(d);
    ASSERT_GE(down.size(), 3u);
    EXPECT_GT(down.front(), 0.4);
    EXPECT_TRUE(std::is_sorted(down.rbegin(), down.rend()));
    EXPECT_EQ(d.shown_text(), "0.400");

    // another unit: converted, 0.400 m² is 4000 cm²; the same value does not count
    d.set_value("999.9", cm2);
    auto converted = count_steps(d);
    ASSERT_FALSE(converted.empty());
    EXPECT_GT(converted.front(), 999.9);
    d.set_value("999.9", cm2);
    EXPECT_FALSE(d.counting());
    d.set_dashes();
    EXPECT_FALSE(d.counting());
    EXPECT_EQ(d.shown_text(), "-----");
}

TEST(WxSegmentDisplay, DashesHaveNoUnit) {
    Fixture f;
    f.display->set_value("1.842", wxString::FromUTF8("m²"));
    f.display->set_dashes();
    EXPECT_EQ(f.display->text(), "-----");
    EXPECT_TRUE(f.display->unit().empty());
}

// a lit pixel close to #47FF86 when bright; none that bright when dimmed;
// unlit segments (#0D2414-ish) always present
TEST(WxSegmentDisplay, PaintsLitAndUnlitSegments) {
    Fixture f;
    SegmentDisplay& d = *f.display;
    wxString const m2 = wxString::FromUTF8("m²");
    d.set_value("8.888", m2);
    EXPECT_TRUE(has_colour_near(d.render(1).ConvertToImage(), 0x47, 0xFF, 0x86, 40));
    d.set_dimmed(true);
    wxImage const dim = d.render(1).ConvertToImage();
    EXPECT_FALSE(has_colour_near(dim, 0x47, 0xFF, 0x86, 40));
    // ~40 % blend of #47FF86 on #050805
    EXPECT_TRUE(has_colour_near(dim, 0x1F, 0x69, 0x37, 12));
    d.set_value("1", m2);
    d.set_dimmed(false);
    EXPECT_TRUE(has_colour_near(d.render(1).ConvertToImage(), 0x0D, 0x24, 0x14, 12));
}

TEST(WxSegmentDisplay, RightAlignsWithBlankLeadingCells) {
    Fixture f;
    f.display->set_value("1.8", wxString::FromUTF8("m²"));
    wxImage const img = f.display->render(1).ConvertToImage();
    // rows below the caption; the first three cells end near x = 121 at the best size
    EXPECT_FALSE(has_colour_near(img, 0x47, 0xFF, 0x86, 40, 0, 115, 20, img.GetHeight()));
    EXPECT_TRUE(has_colour_near(img, 0x47, 0xFF, 0x86, 40, 125, img.GetWidth(), 20, img.GetHeight()));
    // blank cells still show their unlit segments
    EXPECT_TRUE(has_colour_near(img, 0x0D, 0x24, 0x14, 12, 0, 115, 20, img.GetHeight()));
}

TEST(WxSegmentDisplay, OverflowShowsDashes) {
    Fixture f;
    f.display->set_value("123456", wxString::FromUTF8("m²"));
    EXPECT_EQ(f.display->text(), "-----");
    EXPECT_TRUE(f.display->unit().empty());
}

TEST(WxSegmentDisplay, InvalidCharacterShowsDashes) {
    Fixture f;
    f.display->set_value("1,5", wxString::FromUTF8("m²"));
    EXPECT_EQ(f.display->text(), "-----");
    EXPECT_TRUE(f.display->unit().empty());
}

TEST(WxSegmentDisplay, LeadingPointReadsAsZeroPoint) {
    Fixture f;
    f.display->set_value(".5", wxString::FromUTF8("m²"));
    EXPECT_EQ(f.display->text(), "0.5");
}

TEST(WxSegmentDisplay, FitsNarrowWidth) {
    Fixture f;
    SegmentDisplay& d = *f.display;
    wxSize const best = d.GetBestSize();
    d.SetSize(best.x / 2, best.y);
    d.set_value("88888", wxString::FromUTF8("m²"));
    wxImage const img = d.render(1).ConvertToImage();
    EXPECT_TRUE(has_colour_near(img, 0x47, 0xFF, 0x86, 40));
    EXPECT_FALSE(has_colour_near(img, 0x47, 0xFF, 0x86, 40, img.GetWidth() - 2, img.GetWidth()));
    EXPECT_FALSE(has_colour_near(img, 0x47, 0xFF, 0x86, 40, 0, 2, 20, img.GetHeight()));
}

// Qt draws the caption in the right half (Arabic) or from the left (other languages); in the right half its
// right-to-left alignment "right" is the half's left edge, so the Arabic caption starts at the middle
TEST(WxSegmentDisplay, CaptionStartsLeftOrInTheMiddleInArabic) {
    Fixture f;
    auto caption_left = [&] {
        wxImage const img = f.display->render(1).ConvertToImage();
        for (int x = 0; x < img.GetWidth(); ++x) {
            for (int y = 2; y < 18; ++y) {
                if (img.GetGreen(x, y) - img.GetRed(x, y) > 60) return x;
            }
        }
        return -1;
    };
    double const w = f.display->GetSize().x - 1.0, scale = (f.display->GetSize().y - 1.0) / 72;
    EXPECT_NEAR(caption_left(), 0.5 + 10 * scale, 2);
    set_current_language("ar");
    EXPECT_NEAR(caption_left(), 0.5 + w - 10 * scale - w / 2, 3);
    set_current_language("en");
}

TEST(WxSegmentDisplay, ArabicCaptionHasNoLetterSpacing) {
    Fixture f;
    set_current_language("ar");
    EXPECT_EQ(f.display->caption_spacing(1), 0);
    set_current_language("en");
    EXPECT_EQ(f.display->caption_spacing(1), 1);
}

namespace {

/// The widest run of lit green on the row through the "1"'s upper right bar.
int widest_lit_run(wxImage const& img) {
    int const y = static_cast<int>(std::lround(img.GetHeight() * 0.40));
    int widest = 0, run = 0;
    for (int x = 0; x < img.GetWidth(); ++x) {
        bool const lit = std::abs(img.GetRed(x, y) - 0x47) <= 40 && std::abs(img.GetGreen(x, y) - 0xFF) <= 40 &&
                         std::abs(img.GetBlue(x, y) - 0x86) <= 40;
        run = lit ? run + 1 : 0;
        widest = std::max(widest, run);
    }
    return widest;
}

}  // namespace

TEST(WxSegmentDisplay, DrawsAtContentScale) {
    Fixture f;
    SegmentDisplay& d = *f.display;
    d.set_value("1", wxString::FromUTF8("m²"));
    wxBitmap const one = d.render(1), two = d.render(2);
    EXPECT_EQ(two.GetWidth(), 2 * one.GetWidth());
    EXPECT_EQ(two.GetHeight(), 2 * one.GetHeight());
    int const widest1 = widest_lit_run(one.ConvertToImage()), widest2 = widest_lit_run(two.ConvertToImage());
    EXPECT_GE(widest1, 2);
    EXPECT_GE(widest2, 2 * widest1 - 1);
}

TEST(WxSegmentDisplay, SurroundFollowsTheParent) {
    Fixture f;
    for (wxColour const& colour : {wxColour(200, 30, 30), wxColour(30, 30, 200)}) {
        f.frame->SetBackgroundColour(colour);
        wxImage const img = f.display->render(1).ConvertToImage();
        EXPECT_EQ(img.GetRed(0, 0), colour.Red());
        EXPECT_EQ(img.GetGreen(0, 0), colour.Green());
        EXPECT_EQ(img.GetBlue(0, 0), colour.Blue());
    }
}

}  // namespace test_SegmentDisplay
