#include <gtest/gtest.h>

#include <wx/icon.h>
#include <wx/image.h>

#include "ui/AppIcon.h"

namespace test_AppIcon {

namespace {
/// Whether a pixel of the icon, drawn on 64 px, is opaque and within `tolerance` of grey level
/// `level`.
bool has_grey_near(wxIcon const& icon, int level, int tolerance) {
    wxBitmap bitmap;
    bitmap.CopyFromIcon(icon);
    wxImage const image = bitmap.ConvertToImage();
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            if (image.HasAlpha() && image.GetAlpha(x, y) < 255)
                continue;
            int const r = image.GetRed(x, y), g = image.GetGreen(x, y), b = image.GetBlue(x, y);
            if (std::abs(r - level) <= tolerance && std::abs(g - level) <= tolerance &&
                std::abs(b - level) <= tolerance)
                return true;
        }
    }
    return false;
}
}  // namespace

TEST(WxAppIcon, DarkAndLightDiffer) {
    wxIcon const dark =
        app_icon(true).GetIcon(wxSize(64, 64), wxIconBundle::FALLBACK_NEAREST_LARGER);
    wxIcon const light =
        app_icon(false).GetIcon(wxSize(64, 64), wxIconBundle::FALLBACK_NEAREST_LARGER);
    ASSERT_TRUE(dark.IsOk());
    ASSERT_TRUE(light.IsOk());
    EXPECT_TRUE(has_grey_near(dark, 255, 30));
    EXPECT_TRUE(has_grey_near(light, 0, 30));
    EXPECT_FALSE(has_grey_near(dark, 0, 30));
    EXPECT_FALSE(app_icon(false).IsEmpty());
}

}  // namespace test_AppIcon
