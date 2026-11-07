#include <gtest/gtest.h>

#include <wx/dcmemory.h>
#include <wx/frame.h>
#include <wx/graphics.h>

#include "ui/Fonts.h"
#include "ui/MessageLog.h"
#include "ui/SegmentDisplay.h"
#include "ui/Text.h"
#include "ui/Theme.h"

#include <algorithm>
#include <memory>

// The Qt Indicator sets these sizes with QFont::setPixelSize. The expected
// numbers are the ink (lit pixels) of the same text drawn by Qt 6 at the same
// pixel size on this desktop (Noto Sans; QPainter::drawText, white on black).
namespace test_Fonts {

namespace {

/// The size of the lit pixels of `text` drawn in `font`.
wxSize text_ink(wxFont const& font, wxString const& text) {
    wxBitmap bitmap(600, 100, 24);
    {
        wxMemoryDC dc(bitmap);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        gc->SetFont(font, *wxWHITE);
        gc->DrawText(text, 10, 20);
    }
    wxImage const image = bitmap.ConvertToImage();
    int top = image.GetHeight(), bottom = -1, left = image.GetWidth(), right = -1;
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            if (image.GetRed(x, y) <= 100)
                continue;
            top = std::min(top, y);
            bottom = std::max(bottom, y);
            left = std::min(left, x);
            right = std::max(right, x);
        }
    }
    return bottom < top ? wxSize() : wxSize(right - left + 1, bottom - top + 1);
}

/// A frame for the windows whose fonts are measured, destroyed with the test.
struct Frame {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "t");
    ~Frame() { frame->Destroy(); }
};

}  // namespace

TEST(WxFonts, PixelSizeIsQtsEm) {
    Frame f;
    wxFont const base = f.frame->GetFont();
    wxSize const key = text_ink(pixel_font(base, 16), "Import");  // Qt: 51 x 16
    EXPECT_NEAR(key.y, 16, 1);
    EXPECT_NEAR(key.x, 51, 2);
    wxFont bold = base;
    bold.MakeBold();
    wxSize const small_unit = text_ink(pixel_font(bold, 14), wx("m²"));  // Qt: 18 x 12
    EXPECT_NEAR(small_unit.y, 12, 1);
    EXPECT_NEAR(small_unit.x, 18, 1);
    wxSize const unit = text_ink(pixel_font(bold, 27), wx("m²"));  // Qt: 34 x 23
    EXPECT_NEAR(unit.y, 23, 1);
    EXPECT_NEAR(unit.x, 34, 1);
    // wx's own pixel size is a smaller font: the reason for pixel_font
    wxFont line = base;
    line.SetPixelSize(wxSize(0, 16));
    EXPECT_LT(text_ink(line, "Import").y, 14);
}

TEST(WxFonts, ReadoutCaptionIsAsLargeAsQts) {
    Frame f;
    auto* display = new SegmentDisplay(f.frame);
    wxSize const at_one = text_ink(display->caption_font(1), "SURFACE AREA");  // Qt: 62 x 7 (9 px)
    EXPECT_NEAR(at_one.y, 7, 1);
    EXPECT_NEAR(at_one.x, 62, 2);
    wxSize const panel =
        text_ink(display->caption_font(17 / 9.0), "SURFACE AREA");  // Qt: 117 x 12 (17 px)
    EXPECT_NEAR(panel.y, 12, 1);
    EXPECT_NEAR(panel.x, 117, 2);
}

TEST(WxFonts, LogTextIsAsHighAsQts) {
    Frame f;
    auto* log = new MessageLog(f.frame);
    // Qt: 14 high in its fixed font (the desktop's, Hack here; the width depends on the family)
    EXPECT_NEAR(text_ink(log->text_font(), wx("16:15:21  1 object · 1.082 mm/px")).y, 14, 1);
}

TEST(WxFonts, HelpQuestionMarkIsAsLargeAsQts) {
    wxImage const icon = tinted_icon("help", *wxWHITE).GetBitmap(wxSize(20, 20)).ConvertToImage();
    ASSERT_TRUE(icon.IsOk());
    int top = icon.GetHeight(), bottom = -1, left = icon.GetWidth(), right = -1;
    for (int y = 0; y < icon.GetHeight(); ++y) {
        for (int x = 0; x < icon.GetWidth(); ++x) {
            if (icon.HasAlpha() && icon.GetAlpha(x, y) <= 100)
                continue;
            if (!icon.HasAlpha() && icon.GetRed(x, y) <= 100)
                continue;
            top = std::min(top, y);
            bottom = std::max(bottom, y);
            left = std::min(left, x);
            right = std::max(right, x);
        }
    }
    ASSERT_GE(bottom, top);
    EXPECT_NEAR(bottom - top + 1, 13, 1);  // Qt: "?" bold 17 px, 8 x 13
    EXPECT_NEAR(right - left + 1, 8, 1);
    EXPECT_GT(top, 0);  // the whole mark inside the icon
    EXPECT_LT(bottom, 19);
}

}  // namespace test_Fonts
