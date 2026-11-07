#include <Vision/Lens.h>
#include <Vision/Objects.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace test_Lens {

using Vision::LensCamera;

TEST(Lens, DefaultLensIsTodaysScale) {
    EXPECT_NEAR(Vision::lens_scale(LensCamera {}).x, 0.4330, 1e-3);
}
TEST(Lens, CameraConfigGivesTheLensScale) {
    LensCamera const lens {2000, 16, 2.4, 2.4};
    auto s = Vision::mm_per_px(Vision::to_camera_config(lens, 6000), 6000);
    ASSERT_FALSE(s.isError());
    EXPECT_NEAR(s(), Vision::lens_scale(lens).x, 1e-9);
    EXPECT_NEAR(Vision::lens_scale(lens).x, 0.3, 1e-12);
}
// a rotated photo has the other width: the scale is d p / f all the same
TEST(Lens, RotatedWidthKeepsScale) {
    for (int w : {3000, 4000, 517}) {
        auto s = Vision::mm_per_px(Vision::to_camera_config(LensCamera {2000, 16, 2.4, 2.4}, w), w);
        ASSERT_FALSE(s.isError());
        EXPECT_NEAR(s(), 0.3, 1e-12) << w;
    }
}
TEST(Lens, LegacyCameraKeepsItsScale) {
    Vision::CameraConfig const old {1500, 60, 4000};
    auto const lens = Vision::from_legacy(old);
    EXPECT_NEAR(Vision::lens_scale(lens).x, Vision::mm_per_px(old, 4000)(), 1e-9);
}
TEST(Lens, ScaleOfNonSquarePixel) {
    LensCamera const lens {2000, 16, 2.4, 3.2};
    auto const s = Vision::lens_scale(lens);
    EXPECT_NEAR(s.x, 0.3, 1e-12);
    EXPECT_NEAR(s.y, 0.4, 1e-12);
    auto const r = Vision::lens_scale(lens, true);
    EXPECT_NEAR(r.x, 0.4, 1e-12);
    EXPECT_NEAR(r.y, 0.3, 1e-12);
}
TEST(Lens, RotatedLensSwapsAspect) {
    LensCamera const lens {2000, 16, 2.4, 3.2};
    EXPECT_NEAR(Vision::to_camera_config(lens, 1000).pixel_aspect, 4.0 / 3, 1e-12);
    EXPECT_NEAR(Vision::to_camera_config(lens, 1000, true).pixel_aspect, 3.0 / 4, 1e-12);
    EXPECT_NEAR(Vision::mm_per_px(Vision::to_camera_config(lens, 1000), 1000)(), 0.3, 1e-9);
    EXPECT_NEAR(Vision::mm_per_px(Vision::to_camera_config(lens, 1000, true), 1000)(), 0.4, 1e-9);
}
TEST(Lens, SensorPixelUsesLongSides) {
    EXPECT_NEAR(Vision::sensor_pixel_um(15.04, 11.28, 4000, 3000), 3.76, 1e-12);
    EXPECT_NEAR(Vision::sensor_pixel_um(11.28, 15.04, 3000, 4000), 3.76, 1e-12);
    EXPECT_NEAR(Vision::sensor_pixel_um(15.04, 11.28, 1500, 2000), 7.52, 1e-12);
    EXPECT_EQ(Vision::sensor_pixel_um(15.04, 11.28, 0, 0), 0);
    EXPECT_EQ(Vision::sensor_pixel_um(0, 0, 4000, 3000), 0);
}

TEST(MinArea, Pct) {
    EXPECT_NEAR(Vision::min_area_pct(100, 1000, 500, {0.5, 0.5}), 100.0 * 100 / (1000 * 500 * 0.25), 1e-12);
    EXPECT_EQ(Vision::min_area_pct(100, 0, 500, {0.5, 0.5}), 0);
    EXPECT_EQ(Vision::min_area_pct(100, 4000, 3000, {std::nan(""), 1}), 0);
    EXPECT_EQ(Vision::min_area_pct(100, 4000, 3000, {1, INFINITY}), 0);
}
// a minimum area larger than the photo: every object is dropped, not an error
TEST(MinArea, AtMost100) {
    EXPECT_EQ(Vision::min_area_pct(1e6, 100, 100, {0.5, 0.5}), 100);   // 400 % of the photo
    EXPECT_EQ(Vision::min_area_pct(2500, 100, 100, {0.5, 0.5}), 100);  // exactly the photo
}
TEST(MinArea, NonSquare) {
    EXPECT_NEAR(Vision::min_area_pct(100, 1000, 500, {0.5, 1.0}), 100.0 * 100 / (1000 * 500 * 0.5), 1e-12);
}

TEST(MinArea, PresetsAscendFromZero) {
    auto const presets = Vision::min_area_presets();
    ASSERT_EQ(presets.size(), 9u);
    EXPECT_EQ(presets.front(), 0);
    EXPECT_EQ(presets.back(), 50000);
    EXPECT_TRUE(std::is_sorted(presets.begin(), presets.end()));
}
TEST(MinArea, SnapsToTheNearestPreset) {
    EXPECT_EQ(Vision::snap_min_area(100), 100);
    EXPECT_EQ(Vision::snap_min_area(120), 100);
    EXPECT_EQ(Vision::snap_min_area(30), 10);  // 10 and 50 equally near: the first
    EXPECT_EQ(Vision::snap_min_area(-5), 0);
    EXPECT_EQ(Vision::snap_min_area(1e9), 50000);
}

}  // namespace test_Lens
